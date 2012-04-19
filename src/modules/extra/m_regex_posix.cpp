/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
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

public:
	POSIXRegex(const std::string& rx, bool extended) : Regex(rx)
	{
		int flags = (extended ? REG_EXTENDED : 0) | REG_NOSUB;
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
		if (regexec(&regbuf, text.c_str(), 0, NULL, 0) == 0)
		{
			// Bang. :D
			return true;
		}
		return false;
	}
};

class PosixFactory : public RegexFactory
{
 public:
	bool extended;
	PosixFactory(Module* m) : RegexFactory(m, "regex/posix") {}
	Regex* Create(const std::string& expr)
	{
		return new POSIXRegex(expr, extended);
	}
};

class ModuleRegexPOSIX : public Module
{
	PosixFactory ref;
public:
	ModuleRegexPOSIX() : ref(this) {
		ServerInstance->Modules->AddService(ref);
		Implementation eventlist[] = { I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 1);
		OnRehash(NULL);
	}

	Version GetVersion()
	{
		return Version("Regex Provider Module for POSIX Regular Expressions", VF_VENDOR);
	}

	void OnRehash(User* u)
	{
		ConfigReader Conf;
		ref.extended = Conf.ReadFlag("posix", "extended", 0);
	}
};

MODULE_INIT(ModuleRegexPOSIX)
