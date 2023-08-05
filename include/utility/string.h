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

namespace insp
{
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
