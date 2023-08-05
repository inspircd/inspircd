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
}
