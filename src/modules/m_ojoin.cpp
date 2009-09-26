/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
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
				channel->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :%s joined on official network business.",
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
			ServerInstance->SendMode(modes, ServerInstance->FakeClient);
			ServerInstance->PI->SendMode(parameters[0], ServerInstance->Modes->GetLastParseParams(), ServerInstance->Modes->GetLastParseTranslate());
		}
		return CMD_SUCCESS;
	}
};

/** channel mode +Y
 */
class NetworkPrefix : public ModeHandler
{
 public:
	NetworkPrefix(Module* parent) : ModeHandler(parent, 'Y', PARAM_ALWAYS, MODETYPE_CHANNEL)
	{
		list = true;
		prefix = NPrefix;
		levelrequired = 0xFFFFFFFF;
		m_paramtype = TR_NICK;
	}

	ModePair ModeSet(User* source, User* dest, Channel* channel, const std::string &parameter)
	{
		User* x = ServerInstance->FindNick(parameter);
		if (x)
		{
			Membership* m = channel->GetUser(x);
			if (!m)
			{
				return std::make_pair(false, parameter);
			}
			else
			{
				if (m->hasMode('Y'))
				{
					return std::make_pair(true, x->nick);
				}
				else
				{
					return std::make_pair(false, parameter);
				}
			}
		}
		return std::make_pair(false, parameter);
	}

	void RemoveMode(Channel* channel, irc::modestacker* stack)
	{
		const UserMembList* cl = channel->GetUsers();
		std::vector<std::string> mode_junk;
		mode_junk.push_back(channel->name);
		irc::modestacker modestack(false);
		std::deque<std::string> stackresult;

		for (UserMembCIter i = cl->begin(); i != cl->end(); i++)
		{
			if (i->second->hasMode('Y'))
			{
				if (stack)
					stack->Push(this->GetModeChar(), i->first->nick);
				else
					modestack.Push(this->GetModeChar(), i->first->nick);
			}
		}

		if (stack)
			return;

		while (modestack.GetStackedLine(stackresult))
		{
			mode_junk.insert(mode_junk.end(), stackresult.begin(), stackresult.end());
			ServerInstance->SendMode(mode_junk, ServerInstance->FakeClient);
			mode_junk.erase(mode_junk.begin() + 1, mode_junk.end());
		}
	}

	User* FindAndVerify(std::string &parameter, Channel* channel)
	{
		User* theuser = ServerInstance->FindNick(parameter);
		if ((!theuser) || (!channel->HasUser(theuser)))
		{
			parameter.clear();
			return NULL;
		}
		return theuser;
	}

	ModeAction HandleChange(User* source, User* theuser, bool adding, Channel* channel, std::string &parameter)
	{
		Membership* m = channel->GetUser(theuser);
		if (m && adding)
		{
			if (!m->hasMode('Y'))
			{
				parameter = theuser->nick;
				return MODEACTION_ALLOW;
			}
		}
		else if (m && !adding)
		{
			if (m->hasMode('Y'))
			{
				parameter = theuser->nick;
				return MODEACTION_ALLOW;
			}
		}
		return MODEACTION_DENY;
	}

	unsigned int GetPrefixRank()
	{
		return NETWORK_VALUE;
	}

	void RemoveMode(User* user, irc::modestacker* stack)
	{
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		User* theuser = FindAndVerify(parameter, channel);

		if (!theuser)
			return MODEACTION_DENY;

		 // source is a server, or ulined, we'll let them +-Y the user.
		if (source == ServerInstance->FakeClient ||
			((source == theuser) && (!adding)) ||
			(ServerInstance->ULine(source->nick.c_str())) ||
			(ServerInstance->ULine(source->server)) ||
			(!*source->server) ||
			(!IS_LOCAL(source))
			)
		{
			return HandleChange(source, theuser, adding, channel, parameter);
		}
		else
		{
			// bzzzt, wrong answer!
			source->WriteNumeric(482, "%s %s :Only servers may change this mode.", source->nick.c_str(), channel->name.c_str());
			return MODEACTION_DENY;
		}
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
		/* Load config stuff */
		OnRehash(NULL);

		/* Initialise module variables */
		np = new NetworkPrefix(this);

		if (!ServerInstance->Modes->AddMode(np))
		{
			delete np;
			throw ModuleException("Could not add new mode!");
		}

		ServerInstance->AddCommand(&mycommand);

		Implementation eventlist[] = { I_OnUserPreJoin, I_OnUserKick, I_OnUserPart, I_OnUserPreKick, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 5);
	}

	ModResult OnUserPreJoin(User *user, Channel *chan, const char *cname, std::string &privs, const std::string &keygiven)
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

	void OnRehash(User* user)
	{
		ConfigReader Conf;

		if (!np)
		{
			// This is done on module load only
			std::string npre = Conf.ReadValue("ojoin", "prefix", 0);
			NPrefix = npre.empty() ? 0 : npre[0];

			if (NPrefix && ServerInstance->Modes->FindPrefix(NPrefix))
				throw ModuleException("Looks like the +Y prefix you picked for m_ojoin is already in use. Pick another.");
		}

		notice = Conf.ReadFlag("ojoin", "notice", "yes", 0);
		op = Conf.ReadFlag("ojoin", "op", "yes", 0);
	}

	ModResult OnUserPreKick(User* source, Membership* memb, const std::string &reason)
	{
		if ((ServerInstance->ULine(source->nick.c_str())) || (ServerInstance->ULine(source->server)) || (!*source->server))
			return MOD_RES_PASSTHRU;

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
		ServerInstance->Modes->DelMode(np);
		delete np;
	}

	void Prioritize()
	{
		ServerInstance->Modules->SetPriority(this, I_OnUserPreJoin, PRIORITY_FIRST);
	}

	Version GetVersion()
	{
		return Version("Network Buisness Join", VF_COMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleOjoin)

