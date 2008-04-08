/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "xline.h"

/* $ModDesc: Gives /cban, aka C:lines. Think Q:lines, for channels. */

/** Holds a CBAN item
 */
class CBan : public XLine
{
public:
	irc::string matchtext;

	CBan(InspIRCd* Instance, time_t s_time, long d, const char* src, const char* re, const char *ch) : XLine(Instance, s_time, d, src, re, "CBAN")
	{
		this->matchtext = ch;
	}

	~CBan()
	{
	}

	// XXX I shouldn't have to define this
	bool Matches(User *u)
	{
		return false;
	}

	bool Matches(const std::string &s)
	{
		if (matchtext == s)
			return true;
		return false;
	}

	void DisplayExpiry()
	{
		ServerInstance->SNO->WriteToSnoMask('x',"Expiring timed CBan %s (set by %s %ld seconds ago)", this->matchtext.c_str(), this->source, this->duration);
	}

	const char* Displayable()
	{
		return matchtext.c_str();
	}
};

/** An XLineFactory specialized to generate cban pointers
 */
class CBanFactory : public XLineFactory
{
 public:
	CBanFactory(InspIRCd* Instance) : XLineFactory(Instance, "CBAN") { }

	/** Generate a shun
 	*/
	XLine* Generate(time_t set_time, long duration, const char* source, const char* reason, const char* xline_specific_mask)
	{
		return new CBan(ServerInstance, set_time, duration, source, reason, xline_specific_mask);
	}

	bool AutoApplyToUserList(XLine *x)
	{
		return false; // No, we apply to channels.
	}
};

/** Handle /CBAN
 */
class CommandCBan : public Command
{
 public:
	CommandCBan(InspIRCd* Me) : Command(Me, "CBAN", "o", 1)
	{
		this->source = "m_cban.so";
		this->syntax = "<channel> [<duration> :<reason>]";
		TRANSLATE4(TR_TEXT,TR_TEXT,TR_TEXT,TR_END);
	}

	CmdResult Handle(const char* const* parameters, int pcnt, User *user)
	{
		/* syntax: CBAN #channel time :reason goes here */
		/* 'time' is a human-readable timestring, like 2d3h2s. */

		if(pcnt == 1)
		{
			if (ServerInstance->XLines->DelLine(parameters[0], "CBAN", user))
			{
				ServerInstance->SNO->WriteToSnoMask('x',"%s Removed CBan on %s.",user->nick,parameters[0]);
			}
			else
			{
				user->WriteServ("NOTICE %s :*** CBan %s not found in list, try /stats C.",user->nick,parameters[0]);
			}

			return CMD_SUCCESS;
		}
		else if (pcnt >= 2)
		{
			// Adding - XXX todo make this respect <insane> tag perhaps..
			long duration = ServerInstance->Duration(parameters[1]);
			CBan *r = NULL;

			try
			{
				r = new CBan(ServerInstance, ServerInstance->Time(), duration, user->nick, parameters[2], parameters[0]);
			}
			catch (...)
			{
				; // Do nothing. If we get here, the regex was fucked up, and they already got told it fucked up.
			}

			if (r)
			{
				if (ServerInstance->XLines->AddLine(r, user))
				{
					if (!duration)
					{
						ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent CBan for %s.", user->nick, parameters[0]);
					}
					else
					{
						time_t c_requires_crap = duration + ServerInstance->Time();
						ServerInstance->SNO->WriteToSnoMask('x', "%s added timed CBan for %s, expires on %s", user->nick, parameters[0],
						ServerInstance->TimeString(c_requires_crap).c_str());
					}

					ServerInstance->XLines->ApplyLines();
				}
				else
				{
					delete r;
					user->WriteServ("NOTICE %s :*** CBan for %s already exists", user->nick, parameters[0]);
				}
			}
		}

		return CMD_FAILURE;
	}
};

class ModuleCBan : public Module
{
	CommandCBan* mycommand;
	CBanFactory *f;

 public:
	ModuleCBan(InspIRCd* Me) : Module(Me)
	{
		f = new CBanFactory(ServerInstance);
		ServerInstance->XLines->RegisterFactory(f);

		mycommand = new CommandCBan(Me);
		ServerInstance->AddCommand(mycommand);
		Implementation eventlist[] = { I_OnUserPreJoin, I_OnSyncOtherMetaData, I_OnDecodeMetaData, I_OnStats };
		ServerInstance->Modules->Attach(eventlist, this, 4);
	}

	virtual ~ModuleCBan()
	{
		ServerInstance->XLines->UnregisterFactory(f);
	}
	
	virtual int OnStats(char symbol, User* user, string_list &results)
	{
		return 0;
	}

	virtual int OnUserPreJoin(User *user, Channel *chan, const char *cname, std::string &privs)
	{
		XLine *rl = ServerInstance->XLines->MatchesLine("CBAN", cname);

		if (rl)
		{
			// Channel is banned.
			user->WriteServ( "384 %s %s :Cannot join channel, CBANed (%s)", user->nick, cname, rl->reason);
			ServerInstance->SNO->WriteToSnoMask('A', "%s tried to join %s which is CBANed (%s)", user->nick, cname, rl->reason);
			return 1;
		}

		return 0;
	}

	virtual Version GetVersion()
	{
		return Version(1, 2, 0, 1, VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleCBan)

