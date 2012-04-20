/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
	TRERegex(const std::string& rx, InspIRCd* Me) : Regex(rx, Me)
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
	ModuleRegexTRE(InspIRCd* Me) : Module(Me)
	{
		Me->Modules->PublishInterface("RegularExpression", this);
		Implementation eventlist[] = { I_OnRequest };
		Me->Modules->Attach(eventlist, this, 1);
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR | VF_SERVICEPROVIDER, API_VERSION);
	}

	virtual ~ModuleRegexTRE()
	{
		ServerInstance->Modules->UnpublishInterface("RegularExpression", this);
	}

	virtual const char* OnRequest(Request* request)
	{
		if (strcmp("REGEX-NAME", request->GetId()) == 0)
		{
			return "tre";
		}
		else if (strcmp("REGEX", request->GetId()) == 0)
		{
			RegexFactoryRequest* rfr = (RegexFactoryRequest*)request;
			std::string rx = rfr->GetRegex();
			rfr->result = new TRERegex(rx, ServerInstance);
			return "OK";
		}
		return NULL;
	}
};

MODULE_INIT(ModuleRegexTRE)
