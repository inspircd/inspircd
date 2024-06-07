/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022 Sadie Powell <sadie@witchery.services>
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


namespace insp::map
{
	/** Compares two maps and returns a list of the differences.
	 * @param first The first map to compare.
	 * @param second The second map to compare.
	 * @param out The map to store the differences in.
	 */
	template<typename Key, typename Value, typename Compare, template<typename...> typename Map>
	void difference(const Map<Key, Value, Compare>& first, const Map<Key, Value, Compare>& second,
		Map<Key, std::pair<std::optional<Value>, std::optional<Value>>, Compare>& out)
	{
		auto fiter = first.cbegin();
		auto siter = second.cbegin();

		while (fiter != first.end())
		{
			if (siter == second.end())
			{
				// Store the remaining from the first.
				while (fiter != first.end())
				{
					out[fiter->first] = { fiter->second, std::nullopt };
					fiter++;
				}
				return;
			}

			if (fiter->first < siter->first)
			{
				// The key in first is not in second.
				out[fiter->first] = { fiter->second, std::nullopt };
				fiter++;
			}
			else if (siter->first < fiter->first)
			{
				// The key in second is not in first.
				out[siter->first] = { std::nullopt, siter->second };
				siter++;
			}
			else if (fiter->second != siter->second)
			{
				// The key exists in both but the value differs.
				out[fiter->first] = { fiter->second, siter->second };
				fiter++;
				siter++;
			}
			else
			{
				// The key/value is identical in both.
				fiter++;
				siter++;
			}
		}

		// Store the remaining from the second.
		while (siter != second.end())
		{
			out[siter->first] = { std::nullopt, siter->second };
			siter++;
		}
	}
}
