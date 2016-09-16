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

/// $CompilerFlags: execute("pcre-config --cflags" "PCRE_CXXFLAGS")
/// $LinkerFlags: execute("pcre-config --libs" "PCRE_LDFLAGS" "-lpcre")

/// $PackageInfo: require_system("centos") pcre-devel pkgconfig
/// $PackageInfo: require_system("darwin") pcre pkg-config
/// $PackageInfo: require_system("ubuntu") libpcre3-dev pkg-config


#include "inspircd.h"
#include <pcre.h>
#include "modules/regex.h"

#ifdef _WIN32
# pragma comment(lib, "libpcre.lib")
#endif

class PCRERegex : public Regex
{
	pcre* regex;

 public:
	PCRERegex(const std::string& rx) : Regex(rx)
	{
		const char* error;
		int erroffset;
		regex = pcre_compile(rx.c_str(), 0, &error, &erroffset, NULL);
		if (!regex)
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "pcre_compile failed: /%s/ [%d] %s", rx.c_str(), erroffset, error);
			throw RegexException(rx, error, erroffset);
		}
	}

	~PCRERegex()
	{
		pcre_free(regex);
	}

	bool Matches(const std::string& text) CXX11_OVERRIDE
	{
		return (pcre_exec(regex, NULL, text.c_str(), text.length(), 0, 0, NULL, 0) >= 0);
	}
};

class PCREFactory : public RegexFactory
{
 public:
	PCREFactory(Module* m) : RegexFactory(m, "regex/pcre") {}
	Regex* Create(const std::string& expr) CXX11_OVERRIDE
	{
		return new PCRERegex(expr);
	}
};

class ModuleRegexPCRE : public Module
{
 public:
	PCREFactory ref;
	ModuleRegexPCRE() : ref(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Regex Provider Module for PCRE", VF_VENDOR);
	}
};

MODULE_INIT(ModuleRegexPCRE)
