/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Sadie Powell <sadie@witchery.services>
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

/// $PackageInfo: require_system("arch") pcre2
/// $PackageInfo: require_system("centos") pcre2-devel
/// $PackageInfo: require_system("darwin") pcre2
/// $PackageInfo: require_system("debian") libpcre2-dev
/// $PackageInfo: require_system("ubuntu") libpcre2-dev


#include "inspircd.h"
#include "modules/regex.h"

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#ifdef _WIN32
# pragma comment(lib, "pcre2-8.lib")
#endif

class PCRE2Pattern final
	: public Regex::Pattern
{
 private:
	pcre2_code* regex;

 public:
	PCRE2Pattern(const std::string& pattern, uint8_t options)
		: Regex::Pattern(pattern, options)
	{
		int flags = 0;
		if (options & Regex::OPT_CASE_INSENSITIVE)
			flags &= PCRE2_CASELESS;

		int errorcode;
		PCRE2_SIZE erroroffset;
		regex = pcre2_compile(reinterpret_cast<PCRE2_SPTR8>(pattern.c_str()), pattern.length(), flags, &errorcode, &erroroffset, nullptr);
		if (!regex)
		{
			PCRE2_UCHAR errorstr[128];
			pcre2_get_error_message(errorcode, errorstr, sizeof errorstr);
			throw Regex::Exception(pattern, reinterpret_cast<const char*>(errorstr), erroroffset);
		}
	}

	~PCRE2Pattern() override
	{
		pcre2_code_free(regex);
	}

	bool IsMatch(const std::string& text) override
	{
		pcre2_match_data* unused = pcre2_match_data_create(1, nullptr);
		int result = pcre2_match(regex, reinterpret_cast<PCRE2_SPTR8>(text.c_str()), text.length(), 0, 0, unused, nullptr);
		pcre2_match_data_free(unused);
		return result >= 0;
	}
};

class ModuleRegexPCRE2 final
	: public Module
{
 private:
	Regex::SimpleEngine<PCRE2Pattern> regex;

 public:
	ModuleRegexPCRE2()
		: Module(VF_VENDOR, "Provides the pcre2 regular expression engine which uses the PCRE2 library.")
		, regex(this, "pcre2")
	{
	}
};

MODULE_INIT(ModuleRegexPCRE2)
