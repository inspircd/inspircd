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
	PCRERegex(const std::string& rx, InspIRCd* Me) : Regex(rx, Me)
	{
		const char* error;
		int erroffset;
		regex = pcre_compile(rx.c_str(), 0, &error, &erroffset, NULL);
		if (!regex)
		{
			Me->Logs->Log("REGEX", DEBUG, "pcre_compile failed: /%s/ [%d] %s", rx.c_str(), erroffset, error);
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
	ModuleRegexPCRE(InspIRCd* Me) : Module(Me)
	{
		Me->Modules->PublishInterface("RegularExpression", this);
		Implementation eventlist[] = { I_OnRequest };
		Me->Modules->Attach(eventlist, this, 1);
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR | VF_SERVICEPROVIDER, API_VERSION);
	}

	virtual ~ModuleRegexPCRE()
	{
		ServerInstance->Modules->UnpublishInterface("RegularExpression", this);
	}

	virtual const char* OnRequest(Request* request)
	{
		if (strcmp("REGEX-NAME", request->GetId()) == 0)
		{
			return "pcre";
		}
		else if (strcmp("REGEX", request->GetId()) == 0)
		{
			RegexFactoryRequest* rfr = (RegexFactoryRequest*)request;
			std::string rx = rfr->GetRegex();
			rfr->result = new PCRERegex(rx, ServerInstance);
			return "OK";
		}
		return NULL;
	}
};

MODULE_INIT(ModuleRegexPCRE)
