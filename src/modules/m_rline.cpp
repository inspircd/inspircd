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

#include "inspircd.h"
#include "m_regex.h"
#include "xline.h"

static bool ZlineOnMatch = false;
static std::vector<ZLine *> background_zlines;

/* $ModDesc: RLINE: Regexp user banning. */

class RLine : public XLine
{
 public:

	/** Create a R-Line.
	 * @param s_time The set time
	 * @param d The duration of the xline
	 * @param src The sender of the xline
	 * @param re The reason of the xline
	 * @param regex Pattern to match with
	 * @
	 */
	RLine(time_t s_time, long d, std::string src, std::string re, std::string regexs, RegexFactory* rxfactory)
		: XLine(s_time, d, src, re, "R")
	{
		matchtext = regexs;

		/* This can throw on failure, but if it does we DONT catch it here, we catch it and display it
		 * where the object is created, we might not ALWAYS want it to output stuff to snomask x all the time
		 */
		regex = rxfactory->Create(regexs);
	}

	/** Destructor
	 */
	~RLine()
	{
		delete regex;
	}

	bool Matches(User *u)
	{
		if (u->exempt)
			return false;

		std::string compare = u->nick + "!" + u->ident + "@" + u->host + " " + u->fullname;
		return regex->Matches(compare);
	}

	bool Matches(const std::string &compare)
	{
		return regex->Matches(compare);
	}

	void Apply(User* u)
	{
		if (ZlineOnMatch) {
			background_zlines.push_back(new ZLine(ServerInstance->Time(), duration ? expiry - ServerInstance->Time() : 0, ServerInstance->Config->ServerName.c_str(), reason.c_str(), u->GetIPString()));
		}
		DefaultApply(u, "R", false);
	}

	void DisplayExpiry()
	{
		ServerInstance->SNO->WriteToSnoMask('x',"Removing expired R-line %s (set by %s %ld seconds ago)",
			this->matchtext.c_str(), this->source.c_str(), (long int)(ServerInstance->Time() - this->set_time));
	}

	const char* Displayable()
	{
		return matchtext.c_str();
	}

	std::string matchtext;

	Regex *regex;
};


/** An XLineFactory specialized to generate RLine* pointers
 */
class RLineFactory : public XLineFactory
{
 public:
	dynamic_reference<RegexFactory>& rxfactory;
	RLineFactory(dynamic_reference<RegexFactory>& rx) : XLineFactory("R"), rxfactory(rx)
	{
	}
	
	/** Generate a RLine
	 */
	XLine* Generate(time_t set_time, long duration, std::string source, std::string reason, std::string xline_specific_mask)
	{
		if (!rxfactory)
		{
			ServerInstance->SNO->WriteToSnoMask('a', "Cannot create regexes until engine is set to a loaded provider!");
			throw ModuleException("Regex engine not set or loaded!");
		}

		return new RLine(set_time, duration, source, reason, xline_specific_mask, rxfactory);
	}

	~RLineFactory()
	{
	}
};

/** Handle /RLINE
 * Syntax is same as other lines: RLINE regex_goes_here 1d :reason
 */
class CommandRLine : public Command
{
	std::string rxengine;
	RLineFactory& factory;

 public:
	CommandRLine(Module* Creator, RLineFactory& rlf) : Command(Creator,"RLINE", 1, 3), factory(rlf)
	{
		flags_needed = 'o'; this->syntax = "<regex> [<rline-duration>] :<reason>";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{

		if (parameters.size() >= 3)
		{
			// Adding - XXX todo make this respect <insane> tag perhaps..

			long duration = ServerInstance->Duration(parameters[1]);
			XLine *r = NULL;

			try
			{
				r = factory.Generate(ServerInstance->Time(), duration, user->nick.c_str(), parameters[2].c_str(), parameters[0].c_str());
			}
			catch (ModuleException &e)
			{
				ServerInstance->SNO->WriteToSnoMask('a',"Could not add RLINE: %s", e.GetReason());
			}

			if (r)
			{
				if (ServerInstance->XLines->AddLine(r, user))
				{
					if (!duration)
					{
						ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent R-line for %s: %s", user->nick.c_str(), parameters[0].c_str(), parameters[2].c_str());
					}
					else
					{
						time_t c_requires_crap = duration + ServerInstance->Time();
						ServerInstance->SNO->WriteToSnoMask('x', "%s added timed R-line for %s to expire on %s: %s", user->nick.c_str(), parameters[0].c_str(), ServerInstance->TimeString(c_requires_crap).c_str(), parameters[2].c_str());
					}

					ServerInstance->XLines->ApplyLines();
				}
				else
				{
					delete r;
					user->WriteServ("NOTICE %s :*** R-Line for %s already exists", user->nick.c_str(), parameters[0].c_str());
				}
			}
		}
		else
		{
			if (ServerInstance->XLines->DelLine(parameters[0].c_str(), "R", user))
			{
				ServerInstance->SNO->WriteToSnoMask('x',"%s removed R-line on %s",user->nick.c_str(),parameters[0].c_str());
			}
			else
			{
				user->WriteServ("NOTICE %s :*** R-Line %s not found in list, try /stats R.",user->nick.c_str(),parameters[0].c_str());
			}
		}

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_BROADCAST;
	}
};

class ModuleRLine : public Module
{
 private:
	dynamic_reference<RegexFactory> rxfactory;
	RLineFactory f;
	CommandRLine r;
	bool MatchOnNickChange;

 public:
	ModuleRLine() : rxfactory("regex"), f(rxfactory), r(this, f)
	{
	}

	void init()
	{

		ServerInstance->AddCommand(&r);
		ServerInstance->XLines->RegisterFactory(&f);

		Implementation eventlist[] = { I_OnUserConnect, I_OnUserPostNick, I_OnStats, I_OnBackgroundTimer };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));

		if (!rxfactory)
			throw ModuleException("Regex engine not set or loaded!");
	}

	virtual ~ModuleRLine()
	{
		ServerInstance->XLines->DelAll("R");
		ServerInstance->XLines->UnregisterFactory(&f);
	}

	virtual Version GetVersion()
	{
		return Version("RLINE: Regexp user banning.", VF_COMMON | VF_VENDOR, rxfactory ? rxfactory->name : "");
	}

	virtual void OnUserConnect(LocalUser* user)
	{
		// Apply lines on user connect
		XLine *rl = ServerInstance->XLines->MatchesLine("R", user);

		if (rl)
		{
			// Bang. :P
			rl->Apply(user);
		}
	}

	void ReadConfig(ConfigReadStatus&)
	{

		if (!ServerInstance->Config->GetTag("rline")->getBool("zlineonmatch") && ZlineOnMatch)
			background_zlines.clear();

		MatchOnNickChange = ServerInstance->Config->GetTag("rline")->getBool("matchonnickchange");
		ZlineOnMatch = ServerInstance->Config->GetTag("rline")->getBool("zlineonmatch");
		std::string newrxengine = ServerInstance->Config->GetTag("rline")->getString("engine");

		if (newrxengine.empty())
			rxfactory.SetProvider("regex");
		else
			rxfactory.SetProvider("regex/" + newrxengine);
		if (!rxfactory)
		{
			ServerInstance->SNO->WriteToSnoMask('a', "WARNING: Regex engine '%s' is not loaded - R-Line functionality disabled until this is corrected.", newrxengine.c_str());
		}
	}

	virtual ModResult OnStats(char symbol, User* user, string_list &results)
	{
		if (symbol != 'R')
			return MOD_RES_PASSTHRU;

		ServerInstance->XLines->InvokeStats("R", 223, user, results);
		return MOD_RES_DENY;
	}

	virtual void OnUserPostNick(User *user, const std::string &oldnick)
	{
		if (!IS_LOCAL(user))
			return;

		if (!MatchOnNickChange)
			return;

		XLine *rl = ServerInstance->XLines->MatchesLine("R", user);

		if (rl)
		{
			// Bang! :D
			rl->Apply(user);
		}
	}

	virtual void OnBackgroundTimer(time_t curtime)
	{
		if (!ZlineOnMatch) return;
		for (std::vector<ZLine *>::iterator i = background_zlines.begin(); i != background_zlines.end(); i++)
		{
			ZLine *zl = *i;
			if (ServerInstance->XLines->AddLine(zl,NULL))
			{
				ServerInstance->SNO->WriteToSnoMask('x',"Z-line added due to R-line match on *@%s%s%s: %s", 
					zl->ipaddr.c_str(), zl->duration ? " to expire on " : "", zl->duration ? ServerInstance->TimeString(zl->expiry).c_str() : "", zl->reason.c_str());
				ServerInstance->XLines->ApplyLines();
			}
		}
		background_zlines.clear();
	}

};

MODULE_INIT(ModuleRLine)

