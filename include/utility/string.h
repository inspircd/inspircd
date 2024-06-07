/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021, 2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2016 Attila Molnar <attilamolnar@hush.com>
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


#pragma once

#include "utility/string.h"

namespace insp
{
	/** Combines a sequence of elements into a string.
	 * @param sequence A sequence of elements to combine.
	 * @param separator The value to separate the elements with. Defaults to ' ' (space).
	 */
	template<typename Collection, typename Separator = char>
	inline std::string join(const Collection& sequence, Separator separator = ' ')
	{
		std::string joined;
		if (sequence.empty())
			return joined;

		const std::string separatorstr = ConvToStr(separator);
		for (const auto& element : sequence)
			joined.append(ConvToStr(element)).append(separatorstr);

		joined.erase(joined.end() - separatorstr.length());
		joined.shrink_to_fit();
		return joined;
	}

	/** Creates a view of a subsection of a string.
	 * @param str The string to create a view of.
	 * @param begin The position within the string to start the view at.
	 * @param end The position within the string to end the view at.
	 */
	template <typename Iterator>
	std::string_view substring_view(const std::string& str, Iterator begin, Iterator end)
	{
		std::string_view sv = str;
		sv.remove_prefix(std::distance(str.begin(), begin));
		sv.remove_suffix(std::distance(end, str.end()));
		return sv;
	}

	/** Get underlying C string of the string passed as parameter. Useful in template functions.
	 * @param str A `const char*` string.
	 */
	inline const char* tocstr(const char* str)
	{
		return str;
	}

	/** Get underlying C string of the string passed as parameter. Useful in template functions.
	 * @param str A `std::string` string.
	 */
	inline const char* tocstr(const std::string& str)
	{
		return str.c_str();
	}

	/** Check if two strings are equal case insensitively.
	 * @param str1 First string to compare.
	 * @param str2 Second string to compare.
	 * @return True if the strings are equal case-insensitively; otherwise, false.
	 */
	template <typename S1, typename S2>
	inline bool equalsci(const S1& str1, const S2& str2)
	{
		return (!strcasecmp(tocstr(str1), tocstr(str2)));
	}
}
