/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013-2014, 2016-2017, 2019, 2021 Sadie Powell <sadie@witchery.services>
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

/// $CompilerFlags: find_compiler_flags("re2") -std=c++17
/// $LinkerFlags: find_linker_flags("re2")

/// $PackageInfo: require_system("arch") pkgconf re2
/// $PackageInfo: require_system("darwin") pkg-config re2
/// $PackageInfo: require_system("debian" "8.0") libre2-dev pkg-config
/// $PackageInfo: require_system("ubuntu" "15.10") libre2-dev pkg-config


#include "inspircd.h"
#include "modules/regex.h"

#include <re2/re2.h>

class RE2Pattern final
	: public Regex::Pattern
{
 private:
	RE2 regex;

	RE2::Options BuildOptions(uint8_t options)
	{
		RE2::Options re2options;
		re2options.set_case_sensitive(!(options & Regex::OPT_CASE_INSENSITIVE));
		re2options.set_log_errors(false);
		return re2options;
	}

 public:
	RE2Pattern(const std::string& pattern, uint8_t options)
		: Regex::Pattern(pattern, options)
		, regex(pattern, BuildOptions(options))
	{
		if (!regex.ok())
			throw Regex::Exception(pattern, regex.error());
	}

	bool IsMatch(const std::string& text) override
	{
		return RE2::FullMatch(text, regex);
	}
};

class ModuleRegexRE2 final
	: public Module
{
 private:
	Regex::SimpleEngine<RE2Pattern> regex;

 public:
	ModuleRegexRE2()
		: Module(VF_VENDOR, "Provides the re2 regular expression engine which uses the RE2 library.")
		, regex(this, "re2")
	{
	}
};

MODULE_INIT(ModuleRegexRE2)
