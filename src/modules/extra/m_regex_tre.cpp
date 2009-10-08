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
#include <sys/types.h>
#include <tre/regex.h>

/* $ModDesc: Regex Provider Module for TRE Regular Expressions */
/* $CompileFlags: pkgconfincludes("tre","tre/regex.h","") */
/* $LinkerFlags: pkgconflibs("tre","/libtre.so","-ltre") rpath("pkg-config --libs tre") */
/* $ModDep: m_regex.h */

class TRERegexException : public ModuleException
{
public:
	TRERegexException(const std::string& rx, const std::string& error)
		: ModuleException(std::string("Error in regex ") + rx + ": " + error)
	{
	}
};

class TRERegex : public Regex
{
private:
	regex_t regbuf;

public:
	TRERegex(const std::string& rx) : Regex(rx)
	{
		int flags = REG_EXTENDED | REG_NOSUB;
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
			throw TRERegexException(rx, error);
		}
	}

	virtual ~TRERegex()
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

class ModuleRegexTRE : public Module
{
public:
	ModuleRegexTRE() 	{
		ServerInstance->Modules->PublishInterface("RegularExpression", this);
	}

	virtual Version GetVersion()
	{
		return Version("Regex Provider Module for TRE Regular Expressions", VF_COMMON | VF_VENDOR | VF_SERVICEPROVIDER, API_VERSION);
	}

	virtual ~ModuleRegexTRE()
	{
		ServerInstance->Modules->UnpublishInterface("RegularExpression", this);
	}

	void OnRequest(Request& request)
	{
		if (strcmp("REGEX-NAME", request.id) == 0)
		{
			static_cast<RegexNameRequest&>(request).result = "tre";
		}
		else if (strcmp("REGEX", request.id) == 0)
		{
			RegexFactoryRequest& rfr = (RegexFactoryRequest&)request;
			std::string rx = rfr.GetRegex();
			rfr.result = new TRERegex(rx);
		}
	}
};

MODULE_INIT(ModuleRegexTRE)
