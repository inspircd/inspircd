/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "m_regex.h"
#include <sys/types.h>
#include <regex.h>

/* $ModDesc: Regex Provider Module for POSIX Regular Expressions */
/* $ModDep: m_regex.h */

class POSIXRegexException : public ModuleException
{
public:
	POSIXRegexException(const std::string& rx, const std::string& error)
		: ModuleException(std::string("Error in regex ") + rx + ": " + error)
	{
	}
};

class POSIXRegex : public Regex
{
private:
	regex_t regbuf;
	bool irc_lowercase, spaces_to_underscores;

public:
	POSIXRegex(const std::string& rx, RegexFlags reflags, bool extended) : Regex(rx)
	{
		irc_lowercase = reflags & REGEX_IRC_LOWERCASE;
		spaces_to_underscores = reflags & REGEX_SPACES_TO_UNDERSCORES;
		int flags = (extended ? REG_EXTENDED : 0) | ((reflags & REGEX_CASE_INSENSITIVE) ? REG_ICASE : 0) | REG_NOSUB;
		int errcode;
		errcode = regcomp(&regbuf, rx.c_str(), flags);
		if (errcode)
		{
			// Get the error string into a std::string. YUCK this involves at least 2 string copies.
			std::string error;
			char* errbuf;
			size_t sz = regerror(errcode, &regbuf, NULL, 0);
			errbuf = new char[sz + 1];
			memset(errbuf, 0, sz + 1);
			regerror(errcode, &regbuf, errbuf, sz + 1);
			error = errbuf;
			delete[] errbuf;
			regfree(&regbuf);
			throw POSIXRegexException(rx, error);
		}
	}

	virtual ~POSIXRegex()
	{
		regfree(&regbuf);
	}

	virtual bool Matches(const std::string& text)
	{
		std::string matchtext(irc_lowercase ? irc::irc_char_traits::remap(text) : text);
		if(spaces_to_underscores)
			for(std::string::iterator i = matchtext.begin(); i != matchtext.end(); ++i)
				if(*i == ' ')
					*i = '_';
		return regexec(&regbuf, matchtext.c_str(), 0, NULL, 0) == 0;
	}
};

class PosixFactory : public RegexFactory
{
 public:
	bool extended;
	PosixFactory(Module* m) : RegexFactory(m, "regex/posix") {}
	Regex* Create(const std::string& expr, RegexFlags flags)
	{
		return new POSIXRegex(expr, flags, extended);
	}
};

class ModuleRegexPOSIX : public Module
{
	PosixFactory ref;
public:
	ModuleRegexPOSIX() : ref(this) {}

	void init()
	{
		ServerInstance->Modules->AddService(ref);
	}

	void Prioritize()
	{
		// we are a pure service provider, init us first
		ServerInstance->Modules->SetPriority(this, I_ModuleInit, PRIORITY_FIRST);
	}

	Version GetVersion()
	{
		return Version("Regex Provider Module for POSIX Regular Expressions", VF_VENDOR);
	}

	void ReadConfig(ConfigReadStatus&)
	{
		ref.extended = ServerInstance->Config->GetTag("posix")->getBool("extended", true);
	}
};

MODULE_INIT(ModuleRegexPOSIX)
