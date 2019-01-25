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


#include "modules/regex.h"
#include "inspircd.h"

class GlobRegex : public Regex
{
public:
	GlobRegex(const std::string& rx) : Regex(rx)
	{
	}

	bool Matches(const std::string& text) override
	{
		return InspIRCd::Match(text, this->regex_string);
	}
};

class GlobFactory : public RegexFactory
{
 public:
	Regex* Create(const std::string& expr) override
	{
		return new GlobRegex(expr);
	}

	GlobFactory(Module* m) : RegexFactory(m, "regex/glob") {}
};

class ModuleRegexGlob : public Module
{
	GlobFactory gf;
public:
	ModuleRegexGlob()
		: gf(this)
	{
	}

	Version GetVersion() override
	{
		return Version("Regex module using plain wildcard matching.", VF_VENDOR);
	}
};

MODULE_INIT(ModuleRegexGlob)
