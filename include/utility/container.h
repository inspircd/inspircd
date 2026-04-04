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


namespace insp
{
	/** Checks whether an element with the given value is in a container.
	 * @param container The ontainer to find the element in.
	 * @param value The value of the element to look for.
	 * @return True if the element was found in the container; otherwise, false.
	 */
	template <typename Container, typename Value = typename Container::value_type>
	inline bool contains(const Container& container, const Value& value)
	{
		return std::find(container.begin(), container.end(), value) != container.end();
	}

	/** Compares two maps and returns a list of the differences.
	 * @param first The first map to compare.
	 * @param second The second map to compare.
	 * @param out The map to store the differences in.
	 */
	template<typename Key, typename Value, typename Compare, template<typename...> typename Map>
	void map_difference(const Map<Key, Value, Compare>& first, const Map<Key, Value, Compare>& second,
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

	/** Erase a single element from a container by overwriting it with a copy of the last element,
	 * which is then removed. This, in contrast to rase(), does not result in all elements after
	 * the erased element being moved. All iterators, references and pointers to the erased element
	 * and the last element are invalidated
	 * @param container The container to remove the element from.
	 * @param it An iterator to the element to remove.
	 */
	template <typename Container>
	inline void swap_erase(Container& container, const typename Container::iterator& it)
	{
		*it = std::move(container.back());
		container.pop_back();
	}

	/** Find and erase a single element from a vector by overwriting it with a copy of the last
	 * element, which is then removed. This, in contrast to erase(), does not result in all elements
	 * after the erased element being moved. If the given value occurs multiple times, the one with
	 * the lowest index is removed. If the element is found then  all iterators, references and
	 * pointers to the erased element and the last element are invalidated.
	 * @param container The container to remove the element from.
	 * @param value The value to look for and remove.
	 * @return True if the element was found and removed; otherwise, false.
	 */
	template <typename Container>
	inline bool swap_erase(Container& container, const typename Container::value_type& value)
	{
		auto it = std::find(container.begin(), container.end(), value);
		if (it != container.end())
		{
			swap_erase(container, it);
			return true;
		}
		return false;
	}
}
