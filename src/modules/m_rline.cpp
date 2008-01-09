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

	virtual bool Matches(User *u)
	{
		std::string compare = compare.assign(u->nick) + "!" + u->ident + "@" + u->host + " " + u->fullname;

		ServerInstance->Log(DEBUG, "Matching " + matchtext + " against string " + compare);

		if (pcre_exec(regex, NULL, compare.c_str(), compare.length(), 0, 0, NULL, 0) > -1)
		{
			// Bang. :D
			return true;
		}

		return false;
	}

	virtual bool Matches(const std::string&); // Ignored for us

	virtual void Apply(User* u)
	{
		DefaultApply(u, "R", true);
	}

	virtual void DisplayExpiry()
	{
		ServerInstance->SNO->WriteToSnoMask('x',"Expiring timed R-Line %s (set by %s %d seconds ago)", this->matchtext.c_str(), this->source, this->duration);
	}

	virtual const char* Displayable()
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

/*
	if (pcnt >= 3)
	{
		IdentHostPair ih;
		User* find = ServerInstance->FindNick(parameters[0]);
		if (find)
		{
			ih.first = "*";
			ih.second = find->GetIPString();
		}
		else
			ih = ServerInstance->XLines->IdentSplit(parameters[0]);

		if (ServerInstance->HostMatchesEveryone(ih.first+"@"+ih.second,user))
			return CMD_FAILURE;

		if (!strchr(parameters[0],'@'))
		{       
			user->WriteServ("NOTICE %s :*** G-Line must contain a username, e.g. *@%s",user->nick,parameters[0]);
			return CMD_FAILURE;
		}
		else if (strchr(parameters[0],'!'))
		{
			user->WriteServ("NOTICE %s :*** G-Line cannot contain a nickname!",user->nick);
			return CMD_FAILURE;
		}

		long duration = ServerInstance->Duration(parameters[1]);
		GLine* gl = new GLine(ServerInstance, ServerInstance->Time(), duration, user->nick, parameters[2], ih.first.c_str(), ih.second.c_str());
		if (ServerInstance->XLines->AddLine(gl, user))
		{
			if (!duration)
			{
				ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent G-line for %s.",user->nick,parameters[0]);
			}
			else
			{
				time_t c_requires_crap = duration + ServerInstance->Time();
				ServerInstance->SNO->WriteToSnoMask('x',"%s added timed G-line for %s, expires on %s",user->nick,parameters[0],
						ServerInstance->TimeString(c_requires_crap).c_str());
			}

			ServerInstance->XLines->ApplyLines();
		}
		else
		{
			delete gl;
			user->WriteServ("NOTICE %s :*** G-Line for %s already exists",user->nick,parameters[0]);
		}

	}
	else
	{
		if (ServerInstance->XLines->DelLine(parameters[0],"G",user))
		{
			ServerInstance->SNO->WriteToSnoMask('x',"%s Removed G-line on %s.",user->nick,parameters[0]);
		}
		else
		{
			user->WriteServ("NOTICE %s :*** G-line %s not found in list, try /stats g.",user->nick,parameters[0]);
		}
	}
*/

class ModuleRLine : public Module
{
 public:
	ModuleRLine(InspIRCd* Me) : Module(Me)
	{
	}

	virtual ~ModuleRLine()
	{
	}
};

MODULE_INIT(ModuleRLine)

