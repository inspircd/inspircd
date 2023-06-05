/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013-2014, 2016-2017, 2019, 2021-2022 Sadie Powell <sadie@witchery.services>
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

/// $CompilerFlags: -std=c++11 find_compiler_flags("re2" "")
/// $CompilerFlags: require_version("re2" "11") -std=c++17

/// $LinkerFlags: find_linker_flags("re2" "-lre2")

/// $PackageInfo: require_system("arch") pkgconf re2
/// $PackageInfo: require_system("darwin") pkg-config re2
/// $PackageInfo: require_system("debian" "8.0") libre2-dev pkg-config
/// $PackageInfo: require_system("ubuntu" "15.10") libre2-dev pkg-config


#include "inspircd.h"
#include "modules/regex.h"

#ifdef __GNUC__
# pragma GCC diagnostic push
#endif

// Fix warnings about the use of `long long` on C++03 and
// shadowing on GCC.
#if defined __clang__
# pragma clang diagnostic ignored "-Wc++11-long-long"
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wlong-long"
# pragma GCC diagnostic ignored "-Wshadow"
#endif

#include <re2/re2.h>

#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif

class RE2Regex : public Regex
{
	RE2 regexcl;

 public:
	RE2Regex(const std::string& rx) : Regex(rx), regexcl(rx, RE2::Quiet)
	{
		if (!regexcl.ok())
		{
			throw RegexException(rx, regexcl.error());
		}
	}

	bool Matches(const std::string& text) CXX11_OVERRIDE
	{
		return RE2::FullMatch(text, regexcl);
	}
};

class RE2Factory : public RegexFactory
{
 public:
	RE2Factory(Module* m) : RegexFactory(m, "regex/re2") { }
	Regex* Create(const std::string& expr) CXX11_OVERRIDE
	{
		return new RE2Regex(expr);
	}
};

class ModuleRegexRE2 : public Module
{
	RE2Factory ref;

 public:
	ModuleRegexRE2() : ref(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides the re2 regular expression engine which uses the RE2 library.", VF_VENDOR);
	}
};

MODULE_INIT(ModuleRegexRE2)
