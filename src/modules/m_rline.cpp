/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include <pcre.h>
#include "xline.h"

/* $ModDesc: RLINE: Regexp user banning. */
/* $CompileFlags: exec("pcre-config --cflags") */
/* $LinkerFlags: exec("pcre-config --libs") rpath("pcre-config --libs") -lpcre */

#ifdef WINDOWS
#pragma comment(lib, "pcre.lib")
#endif

class CoreExport RLine : public XLine
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
	RLine(InspIRCd* Instance, time_t s_time, long d, const char* src, const char* re, const char* regexs) : XLine(Instance, s_time, d, src, re, "K")
	{
		const char *error;
		int erroffset;

		matchtext = regexs;

		regex = pcre_compile(regexs, 0, &error, &erroffset, NULL);

		if (!regex)
		{
			ServerInstance->SNO->WriteToSnoMask('x',"Error in regular expression: %s at offset %d: %s\n", regexs, erroffset, error);
			throw CoreException("Bad regex pattern.");
		}
	}

	/** Destructor
	 */
	~RLine()
	{
		pcre_free(regex);
	}

	bool Matches(User *u)
	{
		std::string compare = std::string(u->nick) + "!" + u->ident + "@" + u->host + " " + u->fullname;

		ServerInstance->Log(DEBUG, "Matching " + matchtext + " against string " + compare);

		if (pcre_exec(regex, NULL, compare.c_str(), compare.length(), 0, 0, NULL, 0) > -1)
		{
			// Bang. :D
			return true;
		}

		return false;
	}

	bool Matches(const std::string &compare)
	{
		if (pcre_exec(regex, NULL, compare.c_str(), compare.length(), 0, 0, NULL, 0) > -1)
		{
			// Bang. :D
			return true;
		}

		return false;
	}

	void Apply(User* u)
	{
		DefaultApply(u, "R", true);
	}

	void DisplayExpiry()
	{
		ServerInstance->SNO->WriteToSnoMask('x',"Expiring timed R-Line %s (set by %s %d seconds ago)", this->matchtext.c_str(), this->source, this->duration);
	}

	const char* Displayable()
	{
		return matchtext.c_str();
	}

	std::string matchtext;

	pcre *regex;
};


/** An XLineFactory specialized to generate RLine* pointers
 */
class CoreExport RLineFactory : public XLineFactory
{
 public:
	RLineFactory(InspIRCd* Instance) : XLineFactory(Instance, "R") { }

	/** Generate a RLine
	 */
	XLine* Generate(time_t set_time, long duration, const char* source, const char* reason, const char* xline_specific_mask)
	{
		return new RLine(ServerInstance, set_time, duration, source, reason, xline_specific_mask);
	}
};

/** Handle /RLINE
 * Syntax is same as other lines: RLINE regex_goes_here 1d :reason
 */
class CommandRLine : public Command
{
 public:
	CommandRLine (InspIRCd* Instance) : Command(Instance,"RLINE", 'o', 1)
	{
		this->source = "m_rline.so";
	}

	CmdResult Handle (const char** parameters, int pcnt, User *user)
	{

		if (pcnt >= 3)
		{
			// Adding - XXX todo make this respect <insane> tag perhaps..

			long duration = ServerInstance->Duration(parameters[1]);
			RLine *r = NULL;

			try
			{
				r = new RLine(ServerInstance, ServerInstance->Time(), duration, user->nick, parameters[2], parameters[0]);
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
						ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent R-Line for %s.", user->nick, parameters[0]);
					}
					else
					{
						time_t c_requires_crap = duration + ServerInstance->Time();
						ServerInstance->SNO->WriteToSnoMask('x', "%s added timed R-Line for %s, expires on %s", user->nick, parameters[0],
								ServerInstance->TimeString(c_requires_crap).c_str());
					}

					ServerInstance->XLines->ApplyLines();
				}
				else
				{
					delete r;
					user->WriteServ("NOTICE %s :*** R-Line for %s already exists", user->nick, parameters[0]);
				}
			}
		}
		else
		{
			if (ServerInstance->XLines->DelLine(parameters[0], "R", user))
			{
				ServerInstance->SNO->WriteToSnoMask('x',"%s Removed R-Line on %s.",user->nick,parameters[0]);
			}
			else
			{
				// XXX todo implement stats
				user->WriteServ("NOTICE %s :*** R-Line %s not found in list, try /stats g.",user->nick,parameters[0]);
			}
		}

		return CMD_SUCCESS;
	}
};

class ModuleRLine : public Module
{
 private:
	CommandRLine *r;
 public:
	ModuleRLine(InspIRCd* Me) : Module(Me)
	{
		// Create a new command
		r = new CommandRLine(ServerInstance);
		ServerInstance->AddCommand(r);
	}

	virtual ~ModuleRLine()
	{
	}

	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_COMMON|VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(ModuleRLine)

