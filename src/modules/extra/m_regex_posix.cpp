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
	POSIXRegex(const std::string& rx, InspIRCd* Me, bool extended) : Regex(rx, Me)
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

class ModuleRegexPOSIX : public Module
{
private:
	bool extended;
public:
	ModuleRegexPOSIX(InspIRCd* Me) : Module(Me)
	{
		Me->Modules->PublishInterface("RegularExpression", this);
		Implementation eventlist[] = { I_OnRequest, I_OnRehash };
		Me->Modules->Attach(eventlist, this, 2);
		OnRehash(NULL);
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR | VF_SERVICEPROVIDER, API_VERSION);
	}

	virtual ~ModuleRegexPOSIX()
	{
		ServerInstance->Modules->UnpublishInterface("RegularExpression", this);
	}

	virtual void OnRehash(User* u)
	{
		ConfigReader Conf(ServerInstance);
		extended = Conf.ReadFlag("posix", "extended", 0);
	}

	virtual const char* OnRequest(Request* request)
	{
		if (strcmp("REGEX-NAME", request->GetId()) == 0)
		{
			return "posix";
		}
		else if (strcmp("REGEX", request->GetId()) == 0)
		{
			RegexFactoryRequest* rfr = (RegexFactoryRequest*)request;
			std::string rx = rfr->GetRegex();
			rfr->result = new POSIXRegex(rx, ServerInstance, extended);
			return "OK";
		}
		return NULL;
	}
};

MODULE_INIT(ModuleRegexPOSIX)
