/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Jackmcbarn <jackmcbarn@jackmcbarn.no-ip.org>
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
	unsigned const char* map;
	bool irc_lowercase, spaces_to_underscores;
public:
	GlobRegex(const std::string& rx, RegexFlags flags) : Regex(rx)
	{
		if(flags & REGEX_CASE_INSENSITIVE)
			map = ascii_case_insensitive_map;
		else
			map = rfc_case_sensitive_map;
		irc_lowercase = flags & REGEX_IRC_LOWERCASE;
		spaces_to_underscores = flags & REGEX_SPACES_TO_UNDERSCORES;
	}

	virtual ~GlobRegex()
	{
	}

	virtual bool Matches(const std::string& text)
	{
		std::string matchtext(irc_lowercase ? irc::irc_char_traits::remap(text) : text);
		if(spaces_to_underscores)
		{
			for(std::string::iterator i = matchtext.begin(); i != matchtext.end(); ++i)
				if(*i == ' ')
					*i = '_';
		}
		return InspIRCd::Match(matchtext, this->regex_string, map);
	}
};

class GlobFactory : public RegexFactory
{
 public:
	Regex* Create(const std::string& expr, RegexFlags flags)
	{
		return new GlobRegex(expr, flags);
	}

	GlobFactory(Module* m) : RegexFactory(m, "regex/glob") {}
};

class ModuleRegexGlob : public Module
{
	GlobFactory gf;
public:
	ModuleRegexGlob() : gf(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(gf);
	}

	void Prioritize()
	{
		// we are a pure service provider, init us first
		ServerInstance->Modules->SetPriority(this, I_ModuleInit, PRIORITY_FIRST);
	}

	Version GetVersion()
	{
		return Version("Regex module using plain wildcard matching.", VF_VENDOR);
	}
};

MODULE_INIT(ModuleRegexGlob)
