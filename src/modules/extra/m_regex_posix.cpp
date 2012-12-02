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
#include "m_regex.h"
#include <sys/types.h>
#include <regex.h>

/* $ModDesc: Regex Provider Module for POSIX Regular Expressions */
/* $ModDep: m_regex.h */

class POSIXRegexException : public ModuleException
{
public:
	POSIXRegexException(const std::string& rx, const std::string& error)
		: ModuleException("Error in regex " + rx + ": " + error)
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
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		OnRehash(NULL);
	}

	Version GetVersion()
	{
		return Version("Regex Provider Module for POSIX Regular Expressions", VF_VENDOR);
	}

	void OnRehash(User* u)
	{
		ref.extended = ServerInstance->Config->ConfValue("posix")->getBool("extended");
	}
};

MODULE_INIT(ModuleRegexPOSIX)
