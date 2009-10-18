/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *          the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "m_regex.h"
#include "inspircd.h"

/* $ModDesc: Regex module using plain wildcard matching. */

class GlobRegex : public Regex
{
public:
	GlobRegex(const std::string& rx) : Regex(rx)
	{
	}

	virtual ~GlobRegex()
	{
	}

	virtual bool Matches(const std::string& text)
	{
		return InspIRCd::Match(text, this->regex_string);
	}
};

class ModuleRegexGlob : public Module
{
public:
	ModuleRegexGlob() 	{
		ServerInstance->Modules->PublishInterface("RegularExpression", this);
	}

	virtual Version GetVersion()
	{
		return Version("Regex module using plain wildcard matching.", VF_OPTCOMMON | VF_VENDOR);
	}

	virtual ~ModuleRegexGlob()
	{
		ServerInstance->Modules->UnpublishInterface("RegularExpression", this);
	}

	void OnRequest(Request& request)
	{
		if (strcmp("REGEX-NAME", request.id) == 0)
		{
			static_cast<RegexNameRequest&>(request).result = "glob";
		}
		else if (strcmp("REGEX", request.id) == 0)
		{
			RegexFactoryRequest& rfr = (RegexFactoryRequest&)request;
			std::string rx = rfr.GetRegex();
			rfr.result = new GlobRegex(rx);
		}
	}
};

MODULE_INIT(ModuleRegexGlob)
