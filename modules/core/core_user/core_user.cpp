/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014-2015, 2018 Attila Molnar <attilamolnar@hush.com>
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
#include "clientprotocolmsg.h"
#include "modules/hash.h"
#include "utility/string.h"

#include "core_user.h"

enum
{
	// From RFC 1459.
	ERR_NOORIGIN = 409,
	ERR_NOPERMFORHOST = 463,
	ERR_PASSWDMISMATCH = 464,
};

class CommandPass final
	: public SplitCommand
{
public:
	CommandPass(Module* parent)
		: SplitCommand(parent, "PASS", 1, 1)
	{
		penalty = 0;
		syntax = { "<password>" };
		works_before_reg = true;
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		if (user->IsFullyConnected())
		{
			user->CommandFloodPenalty += 1000;
			user->WriteNumeric(ERR_ALREADYREGISTERED, "You may not resend the PASS command");
			return CmdResult::FAILURE;
		}
		user->password = parameters[0];

		return CmdResult::SUCCESS;
	}
};

class CommandPing final
	: public SplitCommand
{
public:
	CommandPing(Module* parent)
		: SplitCommand(parent, "PING", 1)
	{
		syntax = { "<cookie> [<servername>]" };
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		size_t origin = parameters.size() > 1 ? 1 : 0;
		if (parameters[origin].empty())
		{
			user->WriteNumeric(ERR_NOORIGIN, "No origin specified");
			return CmdResult::FAILURE;
		}

		ClientProtocol::Messages::Pong pong(parameters[0], origin ? parameters[1] : ServerInstance->Config->GetServerName());
		user->Send(ServerInstance->GetRFCEvents().pong, pong);
		return CmdResult::SUCCESS;
	}
};

class CommandPong final
	: public Command
{
public:
	CommandPong(Module* parent)
		: Command(parent, "PONG", 1)
	{
		penalty = 0;
		syntax = { "<cookie> [<servername>]" };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		size_t origin = parameters.size() > 1 ? 1 : 0;
		if (parameters[origin].empty())
		{
			user->WriteNumeric(ERR_NOORIGIN, "No origin specified");
			return CmdResult::FAILURE;
		}

		// set the user as alive so they survive to next ping
		LocalUser* localuser = IS_LOCAL(user);
		if (localuser)
		{
			// Increase penalty unless we've sent a PING and this is the reply
			if (localuser->lastping)
				localuser->CommandFloodPenalty += 1000;
			else
				localuser->lastping = 1;
		}
		return CmdResult::SUCCESS;
	}
};

void MessageWrapper::Wrap(const std::string& message, std::string& out)
{
	// If there is a fixed message, it is stored in prefix. Otherwise prefix contains
	// only the prefix, so append the message and the suffix
	out.assign(prefix);
	if (!fixed)
		out.append(message).append(suffix);
}

void MessageWrapper::ReadConfig(const char* prefixname, const char* suffixname, const char* fixedname)
{
	const auto& tag = ServerInstance->Config->ConfValue("options");
	fixed = tag->readString(fixedname, prefix);
	if (!fixed)
	{
		prefix = tag->getString(prefixname);
		suffix = tag->getString(suffixname);
	}
}

class CoreModUser final
	: public Module
{
private:
	CommandAway cmdaway;
	CommandNick cmdnick;
	CommandPart cmdpart;
	CommandPass cmdpass;
	CommandPing cmdping;
	CommandPong cmdpong;
	CommandQuit cmdquit;
	CommandUser cmduser;
	CommandIson cmdison;
	CommandUserhost cmduserhost;
	SimpleUserMode invisiblemode;
	bool clonesonconnect;

public:
	CoreModUser()
		: Module(VF_CORE | VF_VENDOR, "Provides the AWAY, ISON, NICK, PART, PASS, PING, PONG, QUIT, USERHOST, and USER commands")
		, cmdaway(this)
		, cmdnick(this)
		, cmdpart(this)
		, cmdpass(this)
		, cmdping(this)
		, cmdpong(this)
		, cmdquit(this)
		, cmduser(this)
		, cmdison(this)
		, cmduserhost(this)
		, invisiblemode(this, "invisible", 'i')
	{
	}

	ModResult OnPreChangeConnectClass(LocalUser* user, const std::shared_ptr<ConnectClass>& klass, std::optional<Numeric::Numeric>& errnum) override
	{
		bool conndone = user->connected != User::CONN_NONE;
		if (klass->config->getBool("connected", klass->config->getBool("registered", conndone)) != conndone)
		{
			ServerInstance->Logs.Debug("CONNECTCLASS", "The {} connect class is not suitable as it requires that the user is {}.",
				klass->GetName(), conndone ? "not fully connected" : "fully connected");
			return MOD_RES_DENY;
		}

		bool hostmatches = false;
		for (const auto& host : klass->GetHosts())
		{
			if (InspIRCd::MatchCIDR(user->GetAddress(), host) || InspIRCd::MatchCIDR(user->GetRealHost(), host))
			{
				hostmatches = true;
				break;
			}
		}
		if (!hostmatches)
		{
			ServerInstance->Logs.Debug("CONNECTCLASS", "The {} connect class is not suitable as neither the host ({}) nor the IP ({}) matches {}.",
				klass->GetName(), user->GetRealHost(), user->GetAddress(), insp::join(klass->GetHosts()));
			return MOD_RES_DENY;
		}

		if (klass->limit && klass->use_count >= klass->limit)
		{
			ServerInstance->Logs.Debug("CONNECTCLASS", "The {} connect class is not suitable as it has reached its user limit ({}/{}).",
				klass->GetName(), klass->use_count, klass->limit);
			return MOD_RES_DENY;
		}

		if (conndone && !klass->password.empty() && !Hash::CheckPassword(klass->password, klass->passwordhash, user->password))
		{
			const char* error = user->password.empty() ? "one was not provided" : "the provided password was incorrect";
			ServerInstance->Logs.Debug("CONNECTCLASS", "The {} connect class is not suitable as requires a password and {}.",
				klass->GetName(), error);

			errnum = ERR_PASSWDMISMATCH;
			errnum->push_fmt("A password is required and {}.", error);
			return MOD_RES_DENY;
		}

		if (!klass->ports.empty() && !klass->ports.count(user->server_sa.port()))
		{
			ServerInstance->Logs.Debug("CONNECTCLASS", "The {} connect class is not suitable as the connection port ({}) is not any of {}.",
				klass->GetName(), user->server_sa.port(), insp::join(klass->ports));
			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	void OnChangeConnectClass(LocalUser* user, const std::shared_ptr<ConnectClass>& klass, bool force) override
	{
		if (klass->type == ConnectClass::DENY)
		{
			const std::string reason = klass->config->getString("reason", "You are not allowed to connect to this server.", 1);
			user->WriteNumeric(ERR_NOPERMFORHOST, reason);
			ServerInstance->Users.QuitUser(user, reason);
			return;
		}

		// If a user wasn't forced into a class (e.g. via <oper:class>) then we need to check limits.
		if (!force && (clonesonconnect || user->connected != User::CONN_NONE))
		{
			const UserManager::CloneCounts& clonecounts = ServerInstance->Users.GetCloneCounts(user);
			if (klass->maxlocal && clonecounts.local > klass->maxlocal)
			{
				ServerInstance->Users.QuitUser(user, "No more local connections allowed from your host via this connect class.");
				if (klass->maxconnwarn)
				{
					ServerInstance->SNO.WriteToSnoMask('a', "WARNING: maximum local connections for the {} class ({}) exceeded by {}",
						klass->GetName(), klass->maxlocal, user->GetAddress());
				}
				return;
			}

			if (klass->maxglobal && clonecounts.global > klass->maxglobal)
			{
				ServerInstance->Users.QuitUser(user, "No more global connections allowed from your host via this connect class.");
				if (klass->maxconnwarn)
				{
					ServerInstance->SNO.WriteToSnoMask('a', "WARNING: maximum global connections for the {} class ({}) exceeded by {}",
						klass->GetName(), klass->maxglobal, user->GetAddress());
				}
				return;
			}
		}
	}

	void OnUserPart(Membership* memb, std::string& partmessage, CUList& excepts) override
	{
		if (memb->GetRank() < VOICE_VALUE && ServerInstance->Config->RestrictBannedUsers != ServerConfig::BUT_NORMAL && memb->chan->IsBanned(memb->user))
		{
			// The user is banned in the channel and restrictbannedusers is enabled.
			partmessage.clear();
		}
	}

	void ReadConfig(ConfigStatus& status) override
	{
		cmdpart.msgwrap.ReadConfig("prefixpart", "suffixpart", "fixedpart");
		cmdquit.msgwrap.ReadConfig("prefixquit", "suffixquit", "fixedquit");

		const auto& performance = ServerInstance->Config->ConfValue("performance");
		clonesonconnect = performance->getBool("clonesonconnect", true);
	}

	void Prioritize() override
	{
		ServerInstance->Modules.SetPriority(this, I_OnChangeConnectClass, PRIORITY_FIRST);
	}
};

MODULE_INIT(CoreModUser)
