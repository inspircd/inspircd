/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

#ifdef _WIN32
# pragma comment(lib, "libpcre.lib")
#endif

class PCREException : public ModuleException
{
public:
	PCREException(const std::string& rx, const std::string& error, int erroffset)
		: ModuleException("Error in regex " + rx + " at offset " + ConvToStr(erroffset) + ": " + error)
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
	ModuleRegexPCRE() : ref(this) {
		ServerInstance->Modules->AddService(ref);
	}

	Version GetVersion()
	{
		return Version("Regex Provider Module for PCRE", VF_VENDOR);
	}
};

MODULE_INIT(ModuleRegexPCRE)
