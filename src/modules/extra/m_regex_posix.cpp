/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
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

#include <regex.h>
#include <sys/types.h>

class POSIXPattern final
	: public Regex::Pattern
{
private:
	regex_t regex;

public:
	POSIXPattern(const Module* mod, const std::string& pattern, uint8_t options)
		: Regex::Pattern(pattern, options)
	{
		int flags = REG_EXTENDED | REG_NOSUB;
		if (options & Regex::OPT_CASE_INSENSITIVE)
			flags &= REG_ICASE;

		int error = regcomp(&regex, pattern.c_str(), flags);
		if (!error)
			return;

		// Retrieve the size of the error message and allocate a buffer.
		size_t errorsize = regerror(error, &regex, NULL, 0);
		std::vector<char> errormsg(errorsize);

		// Retrieve the error message and free the buffer.
		regerror(error, &regex, &errormsg[0], errormsg.size());
		regfree(&regex);

		throw Regex::Exception(mod, pattern, std::string(&errormsg[0], errormsg.size()));
	}

	~POSIXPattern() override
	{
		regfree(&regex);
	}

	bool IsMatch(const std::string& text) override
	{
		return !regexec(&regex, text.c_str(), 0, NULL, 0);
	}
};

class ModuleRegexPOSIX final
	: public Module
{
private:
	Regex::SimpleEngine<POSIXPattern> regex;

public:
	ModuleRegexPOSIX()
		: Module(VF_VENDOR, "Provides the posix regular expression engine which uses the POSIX.2 regular expression matching system.")
		, regex(this, "posix")
	{
	}
};

MODULE_INIT(ModuleRegexPOSIX)
