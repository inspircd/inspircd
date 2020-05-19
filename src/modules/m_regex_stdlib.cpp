/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2013, 2016, 2018, 2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
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
#include "modules/regex.h"
#include <regex>

class StdRegex : public Regex
{
	std::regex regexcl;

 public:
	StdRegex(const std::string& rx, std::regex::flag_type fltype) : Regex(rx)
	{
		try
		{
			regexcl.assign(rx, fltype | std::regex::optimize);
		}
		catch(const std::regex_error& rxerr)
		{
			throw RegexException(rx, rxerr.what());
		}
	}

	bool Matches(const std::string& text) override
	{
		return std::regex_search(text, regexcl);
	}
};

class StdRegexFactory : public RegexFactory
{
 public:
	std::regex::flag_type regextype;
	StdRegexFactory(Module* m) : RegexFactory(m, "regex/stdregex") {}
	Regex* Create(const std::string& expr) override
	{
		return new StdRegex(expr, regextype);
	}
};

class ModuleRegexStd : public Module
{
 private:
	StdRegexFactory ref;

 public:
	ModuleRegexStd()
		: Module(VF_VENDOR, "Provides a regular expression engine which uses the C++11 std::regex regular expression matching system.")
		, ref(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("stdregex");
		ref.regextype = tag->getEnum("type", std::regex::ECMAScript,
		{
			{ "awk",        std::regex::awk },
			{ "bre",        std::regex::basic },
			{ "ecmascript", std::regex::ECMAScript },
			{ "egrep",      std::regex::egrep },
			{ "ere",        std::regex::extended },
			{ "grep",       std::regex::grep },
		});
	}
};

MODULE_INIT(ModuleRegexStd)
