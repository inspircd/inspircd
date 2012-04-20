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


#include "m_regex.h"
#include "inspircd.h"

/* $ModDesc: Regex module using plain wildcard matching. */

class GlobRegex : public Regex
{
public:
	GlobRegex(const std::string& rx) : Regex(rx)
	{
	}

	virtual ~GlobRegex()
	{
	}

	virtual bool Matches(const std::string& text)
	{
		return InspIRCd::Match(text, this->regex_string);
	}
};

class GlobFactory : public RegexFactory
{
 public:
	Regex* Create(const std::string& expr)
	{
		return new GlobRegex(expr);
	}

	GlobFactory(Module* m) : RegexFactory(m, "regex/glob") {}
};

class ModuleRegexGlob : public Module
{
	GlobFactory gf;
public:
	ModuleRegexGlob() : gf(this) {
		ServerInstance->Modules->AddService(gf);
	}

	Version GetVersion()
	{
		return Version("Regex module using plain wildcard matching.", VF_VENDOR);
	}
};

MODULE_INIT(ModuleRegexGlob)
