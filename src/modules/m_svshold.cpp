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

#include "inspircd.h"
#include "xline.h"

/* $ModDesc: Implements SVSHOLD. Like Q:Lines, but can only be added/removed by Services. */

/** Holds a SVSHold item
 */
class SVSHold : public XLine
{
public:
	irc::string nickname;

	SVSHold(InspIRCd* Instance, time_t s_time, long d, const char* src, const char* re, const char *nick) : XLine(Instance, s_time, d, src, re, "SVSHOLD")
	{
		this->nickname = nick;
	}

	~SVSHold()
	{
	}

	bool Matches(User *u)
	{
		if (u->nick == nickname)
			return true;
		return false;
	}

	bool Matches(const std::string &s)
	{
		if (nickname == s)
			return true;
		return false;
	}

	void DisplayExpiry()
	{
		ServerInstance->SNO->WriteToSnoMask('x',"Removing expired SVSHOLD %s (set by %s %ld seconds ago)", this->nickname.c_str(), this->source, (long int)(ServerInstance->Time() - this->set_time));
	}

	const char* Displayable()
	{
		return nickname.c_str();
	}
};

/** An XLineFactory specialized to generate SVSHOLD pointers
 */
class SVSHoldFactory : public XLineFactory
{
 public:
	SVSHoldFactory(InspIRCd* Instance) : XLineFactory(Instance, "SVSHOLD") { }

	/** Generate a shun
 	*/
	XLine* Generate(time_t set_time, long duration, const char* source, const char* reason, const char* xline_specific_mask)
	{
		return new SVSHold(ServerInstance, set_time, duration, source, reason, xline_specific_mask);
	}
};

/** Handle /SVSHold
 */
class CommandSvshold : public Command
{
 public:
	CommandSvshold(InspIRCd* Me) : Command(Me, "SVSHOLD", "o", 1)
	{
		this->source = "m_svshold.so";
		this->syntax = "<nickname> [<duration> :<reason>]";
		TRANSLATE4(TR_TEXT, TR_TEXT, TR_TEXT, TR_END);
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		/* syntax: svshold nickname time :reason goes here */
		/* 'time' is a human-readable timestring, like 2d3h2s. */

		if (!ServerInstance->ULine(user->server))
		{
			/* don't allow SVSHOLD from non-ulined clients */
			return CMD_FAILURE;
		}

		if (parameters.size() == 1)
		{
			if (ServerInstance->XLines->DelLine(parameters[0].c_str(), "SVSHOLD", user))
			{
				ServerInstance->SNO->WriteToSnoMask('x',"%s Removed SVSHOLD on %s.",user->nick.c_str(),parameters[0].c_str());
			}
			else
			{
				user->WriteServ("NOTICE %s :*** SVSHOLD %s not found in list, try /stats S.",user->nick.c_str(),parameters[0].c_str());
			}

			return CMD_SUCCESS;
		}
		else if (parameters.size() >= 2)
		{
			// Adding - XXX todo make this respect <insane> tag perhaps..
			long duration = ServerInstance->Duration(parameters[1]);
			SVSHold *r = NULL;

			try
			{
				r = new SVSHold(ServerInstance, ServerInstance->Time(), duration, user->nick.c_str(), parameters[2].c_str(), parameters[0].c_str());
			}
			catch (...)
			{
				; // Do nothing.
			}

			if (r)
			{
				if (ServerInstance->XLines->AddLine(r, user))
				{
					ServerInstance->XLines->ApplyLines();
				}
				else
				{
					delete r;
				}
			}
		}

		return CMD_SUCCESS;
	}
};

class ModuleSVSHold : public Module
{
	CommandSvshold *mycommand;
	SVSHoldFactory *s;


 public:
	ModuleSVSHold(InspIRCd* Me) : Module(Me)
	{
		s = new SVSHoldFactory(ServerInstance);
		ServerInstance->XLines->RegisterFactory(s);
		mycommand = new CommandSvshold(Me);
		ServerInstance->AddCommand(mycommand);
		Implementation eventlist[] = { I_OnUserPreNick, I_OnSyncOtherMetaData, I_OnDecodeMetaData, I_OnStats };
		ServerInstance->Modules->Attach(eventlist, this, 4);
	}


	virtual int OnStats(char symbol, User* user, string_list &out)
	{
		if(symbol != 'S')
			return 0;

		ServerInstance->XLines->InvokeStats("SVSHOLD", 210, user, out);
		return 1;
	}

	virtual int OnUserPreNick(User *user, const std::string &newnick)
	{
		XLine *rl = ServerInstance->XLines->MatchesLine("SVSHOLD", newnick);

		if (rl)
		{
			user->WriteServ( "432 %s %s :Services reserved nickname: %s", user->nick.c_str(), newnick.c_str(), rl->reason);
			return 1;
		}

		return 0;
	}

	virtual ~ModuleSVSHold()
	{
		ServerInstance->XLines->DelAll("SVSHOLD");
		ServerInstance->XLines->UnregisterFactory(s);
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleSVSHold)
