/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021-2023 Sadie Powell <sadie@witchery.services>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "iohook.h"
#include "utility/string.h"

class SocketUserIO final
	: public LocalUserIO
	, public StreamSocket
{
private:
	size_t checked_until = 0;

public:
	SocketUserIO(int nfd)
		: StreamSocket(StreamSocket::SS_USER)
	{
		SetFd(nfd);
	}

	void Close() override
	{
		StreamSocket::Close();
	}

	size_t GetRecvQSize() const override
	{
		return this->recvq.size();
	}

	size_t GetSendQSize() const override
	{
		return StreamSocket::GetSendQSize();
	}

	StreamSocket* GetSocket() override
	{
		return this;
	}

	bool OnChangeLocalSocketAddress(const irc::sockets::sockaddrs& sa) override
	{
		memcpy(&user->server_sa, &sa, sizeof(irc::sockets::sockaddrs));
		return true;
	}

	bool OnChangeRemoteSocketAddress(const irc::sockets::sockaddrs& sa) override
	{
		user->ChangeRemoteAddress(sa);
		return !user->quitting;
	}

	void OnDataReady() override
	{
		if (user->quitting || !CheckMaxRecvQ())
			return;

		unsigned long softsendqmax = ULONG_MAX;
		if (!user->HasPrivPermission("users/flood/increased-buffers"))
			softsendqmax = user->GetClass()->softsendqmax;

		unsigned long penaltymax = ULONG_MAX;
		if (!user->HasPrivPermission("users/flood/no-fakelag"))
			penaltymax = user->GetClass()->penaltythreshold * 1000;

		// The cleaned message sent by the user or empty if not found yet.
		std::string line;

		// The position of the most \n character or npos if not found yet.
		std::string::size_type eolpos;

		// The position within the recvq of the current character.
		std::string::size_type qpos;

		while (user->CommandFloodPenalty < penaltymax && GetSendQSize() < softsendqmax)
		{
			// Check the newly received data for an EOL.
			eolpos = recvq.find('\n', checked_until);
			if (eolpos == std::string::npos)
			{
				checked_until = recvq.length();
				return;
			}

			// We've found a line! Clean it up and move it to the line buffer.
			line.reserve(eolpos);
			for (qpos = 0; qpos < eolpos; ++qpos)
			{
				char c = recvq[qpos];
				switch (c)
				{
					case '\0':
						c = ' ';
						break;
					case '\r':
						continue;
				}

				line.push_back(c);
			}

			// just found a newline. Terminate the string, and pull it out of recvq
			recvq.erase(0, eolpos + 1);
			checked_until = 0;

			// TODO should this be moved to when it was inserted in recvq?
			ServerInstance->Stats.Recv += qpos;
			user->bytes_in += qpos;
			user->cmds_in++;

			ServerInstance->Parser.ProcessBuffer(user, line);
			if (user->quitting)
				return;

			// clear() does not reclaim memory associated with the string, so our .reserve() call is safe
			line.clear();
		}

		if (user->CommandFloodPenalty >= penaltymax && !user->GetClass()->fakelag)
			ServerInstance->Users.QuitUser(user, "Excess Flood");
	}

	void OnError(BufferedSocketError e) override
	{
		ServerInstance->Users.QuitUser(user, this->GetError());
	}

	bool Ping() override
	{
		auto* hook = GetIOHook();
		while (hook)
		{
			if (hook->Ping())
				return true; // Client has been pinged.

			auto* middlehook = IOHookMiddle::ToMiddleHook(hook);
			hook = middlehook ? middlehook->GetNextHook() : nullptr;
		}

		return false; // No hook can handle pinging.
	}

	void Process() override
	{
		OnDataReady();
	}

	void Write(const ClientProtocol::SerializedMessage& msg) override
	{
		if (!HasFd() || user->quitting_sendq)
			return;

		if (!user->quitting && !CheckMaxSendQ(msg.length()))
			return;

		// We still want to append data to the sendq of a quitting user,
		// e.g. their ERROR message that says 'closing link'
		WriteData(msg);
	}
};

class CoreModClients final
	: public Module
{
public:
	CoreModClients()
		: Module(VF_CORE | VF_VENDOR, "Accepts connections to the server.")
	{
	}

	ModResult OnAcceptConnection(int nfd, ListenSocket* from, const irc::sockets::sockaddrs& client, const irc::sockets::sockaddrs& server) override
	{
		if (!insp::equalsci(from->bind_tag->getString("type", "clients", 1), "clients"))
			return MOD_RES_PASSTHRU;

		ServerInstance->Logs.Debug("USERS", "New user fd: {}", nfd);

		auto* io = new SocketUserIO(nfd);
		if (!SocketEngine::AddFd(io, FD_WANT_FAST_READ | FD_WANT_EDGE_WRITE))
		{
			ServerInstance->Logs.Debug("USERS", "Internal error on new connection");
			ServerInstance->GlobalCulls.AddItem(static_cast<StreamSocket*>(io));
			return MOD_RES_DENY;
		}

		// If this listener has an IO hook provider set then tell it about the connection
		for (auto i = from->iohookprovs.begin(); i != from->iohookprovs.end(); ++i)
		{
			auto& iohookprovref = *i;
			if (!iohookprovref)
			{
				if (iohookprovref.GetProvider().empty())
					continue;

				const char* hooktype = i == from->iohookprovs.begin() ? "hook" : "sslprofile";
				ServerInstance->Logs.Warning("USERS", "Non-existent I/O hook '{}' in <bind:{}> tag at {}",
					iohookprovref.GetProvider(), hooktype, from->bind_tag->source.str());

				ServerInstance->GlobalCulls.AddItem(static_cast<StreamSocket*>(io));
				return MOD_RES_DENY;
			}

			iohookprovref->OnAccept(io, client, server);

			// IOHook could have encountered a fatal error, e.g. if the TLS ClientHello
			// was already in the queue and there was no common TLS version.
			if (!io->GetError().empty())
			{
				ServerInstance->GlobalCulls.AddItem(static_cast<StreamSocket*>(io));
				return MOD_RES_DENY;
			}
		}

		ServerInstance->Users.AddUser(io, from, client, server);
		return MOD_RES_ALLOW;
	}
};

MODULE_INIT(CoreModClients)
