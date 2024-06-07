/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2020-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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
#include "modules/regex.h"

class GlobPattern final
	: public Regex::Pattern
{
public:
	GlobPattern(const Module* mod, const std::string& pattern, uint8_t options)
		: Regex::Pattern(pattern, options)
	{
	}

	bool IsMatch(const std::string& text) override
	{
		return InspIRCd::Match(text, GetPattern());
	}

	std::optional<Regex::MatchCollection> Matches(const std::string& text) override
	{
		if (!InspIRCd::Match(text, GetPattern()))
			return std::nullopt;

		// The glob engine does not support any kind of capture.
		static const Regex::Captures unusedc;
		static const Regex::NamedCaptures unusednc;

		return Regex::MatchCollection(unusedc, unusednc);
	}
};

class ModuleRegexGlob final
	: public Module
{
private:
	Regex::SimpleEngine<GlobPattern> regex;

public:
	ModuleRegexGlob()
		: Module(VF_VENDOR, "Provides the glob regular expression engine which uses the built-in glob matching system.")
		, regex(this, "glob")
	{
	}
};

MODULE_INIT(ModuleRegexGlob)
