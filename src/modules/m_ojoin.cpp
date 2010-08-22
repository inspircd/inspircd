/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/*
 * Written for InspIRCd-1.2 by Taros on the Tel'Laerad M&D Team
 * <http://tellaerad.net>
 */

#include "inspircd.h"

/* $ModConfig: <ojoin prefix="!" notice="yes" op="yes">
 *  Specify the prefix that +Y will grant here, it should be unused.
 *  Leave prefix empty if you do not wish +Y to grant a prefix
 *  If notice is set to on, upon ojoin, the server will notice
 *  the channel saying that the oper is joining on network business
 *  If op is set to on, it will give them +o along with +Y */
/* $ModDesc: Provides the /ojoin command, which joins a user to a channel on network business, and gives them +Y, which makes them immune to kick / deop and so on. */
/* $ModAuthor: Taros */
/* $ModAuthorMail: taros34@hotmail.com */

/* A note: This will not protect against kicks from services,
 * ulines, or operoverride. */

#define NETWORK_VALUE 9000000

char NPrefix;
bool notice;
bool op;

/** Handle /OJOIN
 */
class CommandOjoin : public Command
{
 public:
	bool active;
	CommandOjoin(Module* parent) : Command(parent,"OJOIN", 1)
	{
		flags_needed = 'o'; Penalty = 0; syntax = "<channel>";
		active = false;
		TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		// Make sure the channel name is allowable.
		if (!ServerInstance->IsChannel(parameters[0].c_str(), ServerInstance->Config->Limits.ChanMax))
		{
			user->WriteServ("NOTICE "+std::string(user->nick)+" :*** Invalid characters in channel name or name too long");
			return CMD_FAILURE;
		}

		active = true;
		Channel* channel = Channel::JoinUser(user, parameters[0].c_str(), false, "", false);
		active = false;

		if (channel)
		{
			ServerInstance->SNO->WriteGlobalSno('a', std::string(user->nick)+" used OJOIN to join "+channel->name);

			if (notice)
			{
				channel = ServerInstance->FindChan(parameters[0]);
				channel->WriteChannelWithServ(ServerInstance->Config->ServerName.c_str(), "NOTICE %s :%s joined on official network business.",
					parameters[0].c_str(), user->nick.c_str());
				ServerInstance->PI->SendChannelNotice(channel, 0, std::string(user->nick) + " joined on official network business.");
			}
		}
		else
		{
			ServerInstance->SNO->WriteGlobalSno('a', std::string(user->nick)+" used OJOIN in "+parameters[0]);
			// they're already in the channel
			std::vector<std::string> modes;
			modes.push_back(parameters[0]);
			modes.push_back("+Y");
			modes.push_back(user->nick);
			ServerInstance->SendGlobalMode(modes, ServerInstance->FakeClient);
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
		fixed_letter = false;
	}

	unsigned int GetPrefixRank()
	{
		return NETWORK_VALUE;
	}

	void AccessCheck(ModePermissionData& perm)
	{
		// remove own privs?
		if (perm.source == perm.user && !perm.mc.adding)
			perm.result = MOD_RES_ALLOW;
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

	void init()
	{
		/* Load config stuff */

		/* Initialise module variables */
		np = new NetworkPrefix(this);

		ServerInstance->Modules->AddService(*np);
		ServerInstance->Modules->AddService(mycommand);

		Implementation eventlist[] = { I_OnCheckJoin, I_OnPermissionCheck };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	void OnCheckJoin(ChannelPermissionData& join)
	{
		if (mycommand.active)
		{
			join.privs += np->GetModeChar();
			if (op)
				join.privs += 'o';
			join.result = MOD_RES_ALLOW;
		}
	}

	void ReadConfig(ConfigReadStatus&)
	{
		ConfigTag* Conf = ServerInstance->Config->GetTag("ojoin");

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

	void OnPermissionCheck(PermissionData& perm)
	{
		if (perm.name != "kick")
			return;
		Membership* memb = perm.chan->GetUser(perm.user);
		// Don't do anything if they're not +Y
		if (!memb || !memb->hasMode(np->GetModeChar()))
			return;

		// Let them do whatever they want to themselves.
		if (perm.source == perm.user)
			return;

		perm.SetReason(":%s 484 %s %s :Can't kick %s as they're on official network business",
			ServerInstance->Config->ServerName.c_str(), perm.source->nick.c_str(),
			memb->chan->name.c_str(), memb->user->nick.c_str());
		perm.result = MOD_RES_DENY;
	}

	~ModuleOjoin()
	{
		delete np;
	}

	void Prioritize()
	{
		ServerInstance->Modules->SetPriority(this, I_OnCheckJoin, PRIORITY_FIRST);
	}

	Version GetVersion()
	{
		return Version("Network Buisness Join", VF_VENDOR);
	}
};

MODULE_INIT(ModuleOjoin)

