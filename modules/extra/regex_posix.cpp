/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020-2023 Sadie Powell <sadie@witchery.services>
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

#ifdef _WIN32
# include "pcre2posix.h"
#else
# include <regex.h>
# include <sys/types.h>
#endif

class POSIXPattern final
	: public Regex::Pattern
{
private:
	regex_t regex;

public:
	POSIXPattern(const Module* mod, const std::string& pattern, uint8_t options)
		: Regex::Pattern(pattern, options)
	{
		int flags = REG_EXTENDED;
		if (options & Regex::OPT_CASE_INSENSITIVE)
			flags |= REG_ICASE;

		int error = regcomp(&regex, pattern.c_str(), flags);
		if (!error)
			return;

		// Retrieve the size of the error message and allocate a buffer.
		size_t errorsize = regerror(error, &regex, nullptr, 0);
		std::vector<char> errormsg(errorsize);

		// Retrieve the error message and free the buffer.
		regerror(error, &regex, errormsg.data(), errormsg.size());
		regfree(&regex);

		throw Regex::Exception(mod, pattern, std::string(errormsg.data(), errormsg.size() - 1));
	}

	~POSIXPattern() override
	{
		regfree(&regex);
	}

	bool IsMatch(const std::string& text) override
	{
		return !regexec(&regex, text.c_str(), 0, nullptr, 0);
	}

	std::optional<Regex::MatchCollection> Matches(const std::string& text) override
	{
		std::vector<regmatch_t> matches(32);
		int result = regexec(&regex, text.c_str(), matches.size(), matches.data(), 0);
		if (result)
			return std::nullopt;

		Regex::Captures captures;
		for (const auto& match : matches)
		{
			if (match.rm_so == -1 || match.rm_eo == -1)
				break;

			captures.emplace_back(text.c_str() + match.rm_so, match.rm_eo - match.rm_so);
		}
		captures.shrink_to_fit();

		// The posix engine does not support named captures.
		static const Regex::NamedCaptures unusednc;

		return Regex::MatchCollection(captures, unusednc);
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
