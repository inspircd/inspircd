/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2026 Sadie Powell <sadie@witchery.services>
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
	/** Converts a number into a string with a binary suffix (e.g. 3,292,549.12 -> 3.14 mebibytes).
	 * @param value The value to convert.
	 * @param bits Whether the value is bits or bytes.
	 */
	inline std::string binary_suffix(uintmax_t value, bool bits = false)
	{
		static const std::map<uintmax_t, const char*, std::greater<>> suffixes = {
			{ 1ULL<<40, "tebi" },
			{ 1ULL<<30, "gibi" },
			{ 1ULL<<20, "mebi" },
			{ 1ULL<<10, "kibi" },
		};
		const auto* unit = bits ? "bits" : "bytes";
		for (const auto& [threshold, suffix] : suffixes)
		{
			if (value >= threshold)
				return FMT::format("{:.5} {}{}", double(value) / threshold, suffix, unit);
		}
		return FMT::format("{} {}", value, unit);
	}

	/** Calculates the percentage of \p total that \p part makes up. This can be called without zero
	 * checks.
	 * @param part The partial value.
	 * @param total The total value.
	 */
	inline double percentage(double part, double total)
	{
		if (total == 0)
			return 0.0;
		return (part / total) * 100.0;
	}
}
