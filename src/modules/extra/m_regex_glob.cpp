/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *          the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "m_regex.h"
#include "inspircd.h"

/* $ModDesc: Regex module using plain wildcard matching. */
/* $ModDep: m_regex.h */

class GlobRegex : public Regex
{
public:
	GlobRegex(const std::string& rx, InspIRCd* Me) : Regex(rx, Me)
	{
	}

	virtual ~GlobRegex()
	{
	}

	virtual bool Matches(const std::string& text)
	{
		return InspIRCd::Match(this->regex_string, text);
	}
};

class ModuleRegexGlob : public Module
{
public:
	ModuleRegexGlob(InspIRCd* Me) : Module(Me)
	{
		Me->Modules->PublishInterface("RegularExpression", this);
		Implementation eventlist[] = { I_OnRequest };
		Me->Modules->Attach(eventlist, this, 1);
	}

	virtual ~ModuleRegexGlob()
	{
		ServerInstance->Modules->UnpublishInterface("RegularExpression", this);
	}

	virtual const char* OnRequest(Request* request)
	{
		if (strcmp("REGEX-NAME", request->GetId()) == 0)
		{
			return "glob";
		}
		else if (strcmp("REGEX", request->GetId()) == 0)
		{
			RegexFactoryRequest* rfr = (RegexFactoryRequest*)request;
			std::string rx = rfr->GetRegex();
			Regex* rxo = (Regex*)new GlobRegex(rx, ServerInstance); // Cast to base class first, since theoretically the pointer could change by that.
			return (const char*)(rxo);
		}
	}
};
