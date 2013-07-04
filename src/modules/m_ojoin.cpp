/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Taros <taros34@hotmail.com>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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

#define NETWORK_VALUE 9000000

char NPrefix;
bool notice;
bool op;

/** Handle /OJOIN
 */
class CommandOjoin : public SplitCommand
{
 public:
	bool active;
	CommandOjoin(Module* parent) :
		SplitCommand(parent, "OJOIN", 1)
	{
		flags_needed = 'o'; Penalty = 0; syntax = "<channel>";
		active = false;
	}

	CmdResult HandleLocal(const std::vector<std::string>& parameters, LocalUser* user)
	{
		// Make sure the channel name is allowable.
		if (!ServerInstance->IsChannel(parameters[0]))
		{
			user->WriteNotice("*** Invalid characters in channel name or name too long");
			return CMD_FAILURE;
		}

		active = true;
		// override is false because we want OnUserPreJoin to run
		Channel* channel = Channel::JoinUser(user, parameters[0], false);
		active = false;

		if (channel)
		{
			ServerInstance->SNO->WriteGlobalSno('a', user->nick+" used OJOIN to join "+channel->name);

			if (notice)
			{
				channel = ServerInstance->FindChan(parameters[0]);
				channel->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :%s joined on official network business.",
					parameters[0].c_str(), user->nick.c_str());
				ServerInstance->PI->SendChannelNotice(channel, 0, user->nick + " joined on official network business.");
			}
		}
		else
		{
			ServerInstance->SNO->WriteGlobalSno('a', user->nick+" used OJOIN in "+parameters[0]);
			// they're already in the channel
			std::vector<std::string> modes;
			modes.push_back(parameters[0]);
			modes.push_back(op ? "+Yo" : "+Y");
			modes.push_back(user->nick);
			if (op)
				modes.push_back(user->nick);
			ServerInstance->Modes->Process(modes, ServerInstance->FakeClient);
		}
		return CMD_SUCCESS;
	}
};

/** channel mode +Y
 */
class NetworkPrefix : public ModeHandler
{
 public:
	NetworkPrefix(Module* parent) : ModeHandler(parent, "official-join", 'Y', PARAM_ALWAYS, MODETYPE_CHANNEL)
	{
		list = true;
		prefix = NPrefix;
		levelrequired = INT_MAX;
		m_paramtype = TR_NICK;
		prefixrank = NETWORK_VALUE;
	}

	ModResult AccessCheck(User* source, Channel* channel, std::string &parameter, bool adding)
	{
		User* theuser = ServerInstance->FindNick(parameter);
		// remove own privs?
		if (source == theuser && !adding)
			return MOD_RES_ALLOW;

		return MOD_RES_PASSTHRU;
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		return MODEACTION_ALLOW;
	}

};

class ModuleOjoin : public Module
{
	NetworkPrefix* np;
	CommandOjoin mycommand;

 public:

	ModuleOjoin()
		: np(NULL), mycommand(this)
	{
	}

	void init() CXX11_OVERRIDE
	{
		/* Load config stuff */
		OnRehash(NULL);

		/* Initialise module variables */
		np = new NetworkPrefix(this);

		ServerInstance->Modules->AddService(*np);
		ServerInstance->Modules->AddService(mycommand);

		Implementation eventlist[] = { I_OnUserPreJoin, I_OnUserPreKick, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven) CXX11_OVERRIDE
	{
		if (mycommand.active)
		{
			privs += 'Y';
			if (op)
				privs += 'o';
			return MOD_RES_ALLOW;
		}

		return MOD_RES_PASSTHRU;
	}

	void OnRehash(User* user) CXX11_OVERRIDE
	{
		ConfigTag* Conf = ServerInstance->Config->ConfValue("ojoin");

		if (!np)
		{
			// This is done on module load only
			std::string npre = Conf->getString("prefix");
			NPrefix = npre.empty() ? 0 : npre[0];

			if (NPrefix && ServerInstance->Modes->FindPrefix(NPrefix))
				throw ModuleException("Looks like the +Y prefix you picked for m_ojoin is already in use. Pick another.");
		}

		notice = Conf->getBool("notice", true);
		op = Conf->getBool("op", true);
	}

	ModResult OnUserPreKick(User* source, Membership* memb, const std::string &reason) CXX11_OVERRIDE
	{
		// Don't do anything if they're not +Y
		if (!memb->hasMode('Y'))
			return MOD_RES_PASSTHRU;

		// Let them do whatever they want to themselves.
		if (source == memb->user)
			return MOD_RES_PASSTHRU;

		source->WriteNumeric(484, source->nick+" "+memb->chan->name+" :Can't kick "+memb->user->nick+" as they're on official network business.");
		return MOD_RES_DENY;
	}

	~ModuleOjoin()
	{
		delete np;
	}

	void Prioritize()
	{
		ServerInstance->Modules->SetPriority(this, I_OnUserPreJoin, PRIORITY_FIRST);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Network Business Join", VF_VENDOR);
	}
};

MODULE_INIT(ModuleOjoin)
