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
		ServerInstance->SNO->WriteToSnoMask('x',"Removing expired CBan %s (set by %s %ld seconds ago)", this->matchtext.c_str(), this->source, (long int)(ServerInstance->Time() - this->set_time));
		ServerInstance->PI->SendSNONotice("x", "Removing expired CBan " + assign(this->matchtext) + " (set by " + std::string(this->source) + " " + ConvToStr(ServerInstance->Time() - this->set_time) + " seconds ago)");
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
	CommandCBan(InspIRCd* Me) : Command(Me, "CBAN", "o", 1, 3)
	{
		this->source = "m_cban.so";
		this->syntax = "<channel> [<duration> :<reason>]";
		TRANSLATE4(TR_TEXT,TR_TEXT,TR_TEXT,TR_END);
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		/* syntax: CBAN #channel time :reason goes here */
		/* 'time' is a human-readable timestring, like 2d3h2s. */

		if (parameters.size() == 1)
		{
			if (ServerInstance->XLines->DelLine(parameters[0].c_str(), "CBAN", user))
			{
				ServerInstance->SNO->WriteToSnoMask('x',"%s removed CBan on %s.",user->nick.c_str(),parameters[0].c_str());
				ServerInstance->PI->SendSNONotice("x", user->nick + " removed CBan on " + parameters[0]);
			}
			else
			{
				user->WriteServ("NOTICE %s :*** CBan %s not found in list, try /stats C.",user->nick.c_str(),parameters[0].c_str());
			}

			return CMD_SUCCESS;
		}
		else if (parameters.size() >= 2)
		{
			// Adding - XXX todo make this respect <insane> tag perhaps..
			long duration = ServerInstance->Duration(parameters[1]);
			CBan *r = NULL;
			const char *reason = (parameters.size() > 2) ? parameters[2].c_str() : "No reason supplied";

			try
			{
				r = new CBan(ServerInstance, ServerInstance->Time(), duration, user->nick.c_str(), reason, parameters[0].c_str());
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
						ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent CBan for %s: %s", user->nick.c_str(), parameters[0].c_str(), reason);
						ServerInstance->PI->SendSNONotice("x", user->nick + " added permenant CBan for " + parameters[0] + ": " + std::string(reason));
					}
					else
					{
						time_t c_requires_crap = duration + ServerInstance->Time();
						ServerInstance->SNO->WriteToSnoMask('x', "%s added timed CBan for %s, expires on %s: %s", user->nick.c_str(), parameters[0].c_str(), ServerInstance->TimeString(c_requires_crap).c_str(), reason);
						ServerInstance->PI->SendSNONotice("x", user->nick + " added timed CBan for " + parameters[0] + ", expires on " + ServerInstance->TimeString(c_requires_crap) + ": " + std::string(reason));
					}

					ServerInstance->XLines->ApplyLines();
				}
				else
				{
					delete r;
					user->WriteServ("NOTICE %s :*** CBan for %s already exists", user->nick.c_str(), parameters[0].c_str());
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
		ServerInstance->XLines->DelAll("CBAN");
		ServerInstance->XLines->UnregisterFactory(f);
	}

	virtual int OnStats(char symbol, User* user, string_list &out)
	{
		if (symbol != 'C')
			return 0;

		ServerInstance->XLines->InvokeStats("CBAN", 210, user, out);
		return 1;
	}

	virtual int OnUserPreJoin(User *user, Channel *chan, const char *cname, std::string &privs, const std::string &keygiven)
	{
		XLine *rl = ServerInstance->XLines->MatchesLine("CBAN", cname);

		if (rl)
		{
			// Channel is banned.
			user->WriteServ( "384 %s %s :Cannot join channel, CBANed (%s)", user->nick.c_str(), cname, rl->reason);
			ServerInstance->SNO->WriteToSnoMask('a', "%s tried to join %s which is CBANed (%s)", user->nick.c_str(), cname, rl->reason);
			ServerInstance->PI->SendSNONotice("A", user->nick + " tried to join " + std::string(cname) + " which is CBANed (" + std::string(rl->reason) + ")");
			return 1;
		}

		return 0;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleCBan)

