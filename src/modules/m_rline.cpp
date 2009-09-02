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
#include "m_regex.h"
#include "xline.h"

static Module* rxengine = NULL;
static Module* mymodule = NULL; /* Needed to let RLine send request! */

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
	RLine(InspIRCd* Instance, time_t s_time, long d, const char* src, const char* re, const char* regexs) : XLine(Instance, s_time, d, src, re, "R")
	{
		matchtext = regexs;

		if (!rxengine)
		{
			ServerInstance->SNO->WriteToSnoMask('x', "Cannot create regexes until engine is set to a loaded provider!");
			throw ModuleException("Regex engine not set or loaded!");
		}

		/* This can throw on failure, but if it does we DONT catch it here, we catch it and display it
		 * where the object is created, we might not ALWAYS want it to output stuff to snomask x all the time
		 */
		regex = RegexFactoryRequest(mymodule, rxengine, regexs).Create();
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
		DefaultApply(u, "R", true);
	}

	void DisplayExpiry()
	{
		ServerInstance->SNO->WriteToSnoMask('x',"Removing expired R-Line %s (set by %s %ld seconds ago)", this->matchtext.c_str(), this->source, (long int)(ServerInstance->Time() - this->set_time));
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
	RLineFactory(InspIRCd* Instance) : XLineFactory(Instance, "R")
	{
	}

	/** Generate a RLine
	 */
	XLine* Generate(time_t set_time, long duration, const char* source, const char* reason, const char* xline_specific_mask)
	{
		return new RLine(ServerInstance, set_time, duration, source, reason, xline_specific_mask);
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

 public:
	CommandRLine (InspIRCd* Instance) : Command(Instance,"RLINE", "o", 1, 3)
	{
		this->source = "m_rline.so";
		this->syntax = "<regex> [<rline-duration>] :<reason>";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{

		if (parameters.size() >= 3)
		{
			// Adding - XXX todo make this respect <insane> tag perhaps..

			long duration = ServerInstance->Duration(parameters[1]);
			RLine *r = NULL;

			try
			{
				r = new RLine(ServerInstance, ServerInstance->Time(), duration, user->nick.c_str(), parameters[2].c_str(), parameters[0].c_str());
			}
			catch (ModuleException &e)
			{
				ServerInstance->SNO->WriteToSnoMask('x',"Could not add RLINE: %s", e.GetReason());
			}

			if (r)
			{
				if (ServerInstance->XLines->AddLine(r, user))
				{
					if (!duration)
					{
						ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent R-Line for %s: %s", user->nick.c_str(), parameters[0].c_str(), parameters[2].c_str());
					}
					else
					{
						time_t c_requires_crap = duration + ServerInstance->Time();
						ServerInstance->SNO->WriteToSnoMask('x', "%s added timed R-Line for %s, expires on %s: %s", user->nick.c_str(), parameters[0].c_str(), ServerInstance->TimeString(c_requires_crap).c_str(), parameters[2].c_str());
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
				ServerInstance->SNO->WriteToSnoMask('x',"%s Removed R-Line on %s.",user->nick.c_str(),parameters[0].c_str());
			}
			else
			{
				user->WriteServ("NOTICE %s :*** R-Line %s not found in list, try /stats R.",user->nick.c_str(),parameters[0].c_str());
			}
		}

		return CMD_SUCCESS;
	}
};

class ModuleRLine : public Module
{
 private:
	CommandRLine r;
	RLineFactory f;
	bool MatchOnNickChange;
	std::string RegexEngine;

 public:
	ModuleRLine(InspIRCd* Me) : Module(Me), r(Me), f(Me)
	{
		mymodule = this;
		OnRehash(NULL);

		Me->Modules->UseInterface("RegularExpression");

		ServerInstance->AddCommand(&r);
		ServerInstance->XLines->RegisterFactory(&f);

		Implementation eventlist[] = { I_OnUserConnect, I_OnRehash, I_OnUserPostNick, I_OnLoadModule, I_OnStats };
		ServerInstance->Modules->Attach(eventlist, this, 5);

	}

	virtual ~ModuleRLine()
	{
		ServerInstance->Modules->DoneWithInterface("RegularExpression");
		ServerInstance->XLines->DelAll("R");
		ServerInstance->XLines->UnregisterFactory(&f);
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}

	virtual void OnUserConnect(User* user)
	{
		// Apply lines on user connect
		XLine *rl = ServerInstance->XLines->MatchesLine("R", user);

		if (rl)
		{
			// Bang. :P
			rl->Apply(user);
		}
	}

	virtual void OnRehash(User *user)
	{
		ConfigReader Conf(ServerInstance);

		MatchOnNickChange = Conf.ReadFlag("rline", "matchonnickchange", 0);
		std::string newrxengine = Conf.ReadValue("rline", "engine", 0);

		if (!RegexEngine.empty())
		{
			if (RegexEngine == newrxengine)
				return;

			ServerInstance->SNO->WriteToSnoMask('x', "Dumping all R-Lines due to regex engine change (was '%s', now '%s')", RegexEngine.c_str(), newrxengine.c_str());
			ServerInstance->XLines->DelAll("R");
		}
		rxengine = 0;
		RegexEngine = newrxengine;
		modulelist* ml = ServerInstance->Modules->FindInterface("RegularExpression");
		if (ml)
		{
			for (modulelist::iterator i = ml->begin(); i != ml->end(); ++i)
			{
				if (RegexNameRequest(this, *i).Send() == newrxengine)
				{
					ServerInstance->SNO->WriteToSnoMask('x', "R-Line now using engine '%s'", RegexEngine.c_str());
					rxengine = *i;
				}
			}
		}
		if (!rxengine)
		{
			ServerInstance->SNO->WriteToSnoMask('x', "WARNING: Regex engine '%s' is not loaded - R-Line functionality disabled until this is corrected.", RegexEngine.c_str());
		}
	}

	virtual int OnStats(char symbol, User* user, string_list &results)
	{
		if (symbol != 'R')
			return 0;

		ServerInstance->XLines->InvokeStats("R", 223, user, results);
		return 1;
	}

	virtual void OnLoadModule(Module* mod, const std::string& name)
	{
		if (ServerInstance->Modules->ModuleHasInterface(mod, "RegularExpression"))
		{
			std::string rxname = RegexNameRequest(this, mod).Send();
			if (rxname == RegexEngine)
			{
				ServerInstance->SNO->WriteToSnoMask('x', "R-Line now using engine '%s'", RegexEngine.c_str());
				rxengine = mod;
			}
		}
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

};

MODULE_INIT(ModuleRLine)

