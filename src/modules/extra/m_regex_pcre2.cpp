/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021-2024 Sadie Powell <sadie@witchery.services>
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

/// $CompilerFlags: find_compiler_flags("libpcre2-8")
/// $LinkerFlags: find_linker_flags("libpcre2-8")

/// $PackageInfo: require_system("alpine") pcre2-devel
/// $PackageInfo: require_system("arch") pcre2
/// $PackageInfo: require_system("darwin") pcre2
/// $PackageInfo: require_system("debian~") libpcre2-dev
/// $PackageInfo: require_system("rhel~") pcre2-devel


#include "inspircd.h"
#include "modules/regex.h"

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#ifdef _WIN32
# pragma comment(lib, "pcre2-8.lib")
#endif

class PCREPattern final
	: public Regex::Pattern
{
private:
	pcre2_code* regex;

public:
	PCREPattern(const Module* mod, const std::string& pattern, uint8_t options)
		: Regex::Pattern(pattern, options)
	{
		int flags = 0;
		if (options & Regex::OPT_CASE_INSENSITIVE)
			flags |= PCRE2_CASELESS;

		int errorcode;
		PCRE2_SIZE erroroffset;
		regex = pcre2_compile(reinterpret_cast<PCRE2_SPTR8>(pattern.c_str()), pattern.length(), flags, &errorcode, &erroroffset, nullptr);
		if (!regex)
		{
			PCRE2_UCHAR errorstr[128];
			pcre2_get_error_message(errorcode, errorstr, sizeof errorstr);
			throw Regex::Exception(mod, pattern, reinterpret_cast<const char*>(errorstr), erroroffset);
		}
	}

	~PCREPattern() override
	{
		pcre2_code_free(regex);
	}

	bool IsMatch(const std::string& text) override
	{
		pcre2_match_data* unused = pcre2_match_data_create_from_pattern(regex, nullptr);
		int result = pcre2_match(regex, reinterpret_cast<PCRE2_SPTR8>(text.c_str()), text.length(), 0, 0, unused, nullptr);
		pcre2_match_data_free(unused);
		return result >= 0;
	}

	std::optional<Regex::MatchCollection> Matches(const std::string& text) override
	{
		pcre2_match_data* data = pcre2_match_data_create_from_pattern(regex, nullptr);
		int result = pcre2_match(regex, reinterpret_cast<PCRE2_SPTR8>(text.c_str()), text.length(), 0, 0, data, nullptr);
		if (result < 0)
		{
			pcre2_match_data_free(data);
			return std::nullopt;
		}

		PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(data);

		uint32_t capturecount;
		Regex::Captures captures;
		if (!pcre2_pattern_info(regex, PCRE2_INFO_CAPTURECOUNT, &capturecount) && capturecount)
		{
			for (uint32_t idx = 0; idx <= capturecount; ++idx)
			{
				PCRE2_UCHAR* bufferptr;
				PCRE2_SIZE bufferlen;
				if (!pcre2_substring_get_bynumber(data, idx, &bufferptr, &bufferlen))
					captures.emplace_back(reinterpret_cast<const char*>(bufferptr), bufferlen);
			}
		}

		uint32_t namedcapturecount;
		Regex::NamedCaptures namedcaptures;
		if (!pcre2_pattern_info(regex, PCRE2_INFO_NAMECOUNT, &namedcapturecount) && namedcapturecount)
		{
			uint32_t nameentrysize;
			PCRE2_SPTR nametable;
			if (!pcre2_pattern_info(regex, PCRE2_INFO_NAMEENTRYSIZE, &nameentrysize)
				&& !pcre2_pattern_info(regex, PCRE2_INFO_NAMETABLE, &nametable))
			{
				for (uint32_t idx = 0; idx < namedcapturecount; ++idx)
				{
					int matchidx = (nametable[0] << 8) | nametable[1];
					const std::string matchname(reinterpret_cast<const char*>(nametable + 2), nameentrysize - 3);
					const std::string matchvalue(text.c_str() + ovector[2 * matchidx], ovector[ 2 * matchidx + 1] - ovector[2 * matchidx]);
					namedcaptures.emplace(matchname, matchvalue);
					nametable += nameentrysize;
				}
			}
		}

		pcre2_match_data_free(data);
		return Regex::MatchCollection(captures, namedcaptures);
	}
};

class ModuleRegexPCRE final
	: public Module
{
private:
	Regex::SimpleEngine<PCREPattern> regex;

public:
	ModuleRegexPCRE()
		: Module(VF_VENDOR, "Provides the pcre regular expression engine which uses the PCRE2 library.")
		, regex(this, "pcre")
	{
	}

	void init() override
	{
		std::vector<char> version(16);
		if (pcre2_config(PCRE2_CONFIG_VERSION, version.data()) < 0)
			return;

		ServerInstance->Logs.Normal(MODNAME, "Module was compiled against PCRE2 version {}.{} and is running against version {}",
			PCRE2_MAJOR, PCRE2_MINOR, version.data());
	}
};

MODULE_INIT(ModuleRegexPCRE)
