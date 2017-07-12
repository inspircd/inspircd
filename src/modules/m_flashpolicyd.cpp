/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
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

class FlashPDSocket;

namespace
{
	insp::intrusive_list<FlashPDSocket> sockets;
	std::string policy_reply;
	const std::string expected_request("<policy-file-request/>\0", 23);
}

class FlashPDSocket : public BufferedSocket, public Timer, public insp::intrusive_list_node<FlashPDSocket>
{
	/** True if this object is in the cull list
	 */
	bool waitingcull;

	bool Tick(time_t currtime) CXX11_OVERRIDE
	{
		AddToCull();
		return false;
	}

 public:
	FlashPDSocket(int newfd, unsigned int timeoutsec)
		: BufferedSocket(newfd)
		, Timer(timeoutsec)
		, waitingcull(false)
	{
		ServerInstance->Timers.AddTimer(this);
	}

	~FlashPDSocket()
	{
		sockets.erase(this);
	}

	void OnError(BufferedSocketError) CXX11_OVERRIDE
	{
		AddToCull();
	}

	void OnDataReady() CXX11_OVERRIDE
	{
		if (recvq == expected_request)
			WriteData(policy_reply);
		AddToCull();
	}

	void AddToCull()
	{
		if (waitingcull)
			return;

		waitingcull = true;
		Close();
		ServerInstance->GlobalCulls.AddItem(this);
	}
};

class ModuleFlashPD : public Module
{
	unsigned int timeout;

 public:
	ModResult OnAcceptConnection(int nfd, ListenSocket* from, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server) CXX11_OVERRIDE
	{
		if (from->bind_tag->getString("type") != "flashpolicyd")
			return MOD_RES_PASSTHRU;

		if (policy_reply.empty())
			return MOD_RES_DENY;

		sockets.push_front(new FlashPDSocket(nfd, timeout));
		return MOD_RES_ALLOW;
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("flashpolicyd");
		timeout = tag->getInt("timeout", 5, 1);
		std::string file = tag->getString("file");

		if (!file.empty())
		{
			try
			{
				FileReader reader(file);
				policy_reply = reader.GetString();
			}
			catch (CoreException&)
			{
				const std::string error_message = "A file was specified for FlashPD, but it could not be loaded.";
				ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, error_message);
				ServerInstance->SNO->WriteGlobalSno('a', error_message);
				policy_reply.clear();
			}
			return;
		}

		// A file was not specified. Set the default setting.
		// We allow access to all client ports by default
		std::string to_ports;
		for (std::vector<ListenSocket*>::const_iterator i = ServerInstance->ports.begin(); i != ServerInstance->ports.end(); ++i)
		{
				ListenSocket* ls = *i;
				if (ls->bind_tag->getString("type", "clients") != "clients" || ls->bind_tag->getString("ssl", "plaintext") != "plaintext")
					continue;

				to_ports.append(ConvToStr(ls->bind_port)).push_back(',');
		}

		if (to_ports.empty())
		{
			policy_reply.clear();
			return;
		}

		to_ports.erase(to_ports.size() - 1);

		policy_reply =
"<?xml version=\"1.0\"?>\
<!DOCTYPE cross-domain-policy SYSTEM \"/xml/dtds/cross-domain-policy.dtd\">\
<cross-domain-policy>\
<site-control permitted-cross-domain-policies=\"master-only\"/>\
<allow-access-from domain=\"*\" to-ports=\"" + to_ports + "\" />\
</cross-domain-policy>";
	}

	CullResult cull() CXX11_OVERRIDE
	{
		for (insp::intrusive_list<FlashPDSocket>::const_iterator i = sockets.begin(); i != sockets.end(); ++i)
		{
			FlashPDSocket* sock = *i;
			sock->AddToCull();
		}
		return Module::cull();
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Flash Policy Daemon. Allows Flash IRC clients to connect", VF_VENDOR);
	}
};

MODULE_INIT(ModuleFlashPD)
