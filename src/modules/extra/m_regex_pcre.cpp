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
#include <pcre.h>
#include "m_regex.h"

/* $ModDesc: Regex Provider Module for PCRE */
/* $ModDep: m_regex.h */
/* $CompileFlags: exec("pcre-config --cflags") */
/* $LinkerFlags: exec("pcre-config --libs") rpath("pcre-config --libs") -lpcre */

#ifdef WINDOWS
#pragma comment(lib, "pcre.lib")
#endif

class PCREException : public ModuleException
{
public:
	PCREException(const std::string& rx, const std::string& error, int erroffset)
		: ModuleException(std::string("Error in regex ") + rx + " at offset " + ConvToStr(erroffset) + ": " + error)
	{
	}
};

class PCRERegex : public Regex
{
private:
	pcre* regex;

public:
	PCRERegex(const std::string& rx) : Regex(rx)
	{
		const char* error;
		int erroffset;
		regex = pcre_compile(rx.c_str(), 0, &error, &erroffset, NULL);
		if (!regex)
		{
			ServerInstance->Logs->Log("REGEX", DEBUG, "pcre_compile failed: /%s/ [%d] %s", rx.c_str(), erroffset, error);
			throw PCREException(rx, error, erroffset);
		}
	}

	virtual ~PCRERegex()
	{
		pcre_free(regex);
	}

	virtual bool Matches(const std::string& text)
	{
		if (pcre_exec(regex, NULL, text.c_str(), text.length(), 0, 0, NULL, 0) > -1)
		{
			// Bang. :D
			return true;
		}
		return false;
	}
};

class PCREFactory : public RegexFactory
{
 public:
	PCREFactory(Module* m) : RegexFactory(m, "regex/pcre") {}
	Regex* Create(const std::string& expr)
	{
		return new PCRERegex(expr);
	}
};

class ModuleRegexPCRE : public Module
{
public:
	PCREFactory ref;
	ModuleRegexPCRE() : ref(this) {}

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
		return Version("Regex Provider Module for PCRE", VF_VENDOR);
	}
};

MODULE_INIT(ModuleRegexPCRE)
