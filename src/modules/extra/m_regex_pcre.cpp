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

class ModuleRegexPCRE : public Module
{
public:
	ModuleRegexPCRE() 	{
		ServerInstance->Modules->PublishInterface("RegularExpression", this);
	}

	virtual Version GetVersion()
	{
		return Version("Regex Provider Module for PCRE", VF_COMMON | VF_VENDOR | VF_SERVICEPROVIDER, API_VERSION);
	}

	virtual ~ModuleRegexPCRE()
	{
		ServerInstance->Modules->UnpublishInterface("RegularExpression", this);
	}

	void OnRequest(Request& request)
	{
		if (strcmp("REGEX-NAME", request.id) == 0)
		{
			static_cast<RegexNameRequest&>(request).result = "pcre";
		}
		else if (strcmp("REGEX", request.id) == 0)
		{
			RegexFactoryRequest& rfr = (RegexFactoryRequest&)request;
			std::string rx = rfr.GetRegex();
			rfr.result = new PCRERegex(rx);
		}
	}
};

MODULE_INIT(ModuleRegexPCRE)
