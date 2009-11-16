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

class GlobFactory : public RegexFactory
{
 public:
	Regex* Create(const std::string& expr)
	{
		return new GlobRegex(expr);
	}

	GlobFactory(Module* m) : RegexFactory(m, "regex/glob") {}
};

class ModuleRegexGlob : public Module
{
	GlobFactory gf;
public:
	ModuleRegexGlob() : gf(this) {
		ServerInstance->Modules->AddService(gf);
	}

	Version GetVersion()
	{
		return Version("Regex module using plain wildcard matching.", VF_OPTCOMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleRegexGlob)
