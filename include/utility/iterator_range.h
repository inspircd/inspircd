/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020-2022 Sadie Powell <sadie@witchery.services>
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
	template <typename Iterator>
	class iterator_range;

	/** Returns a range containing all elements equivalent to \p value.
	 * @param collection The collection to search within.
	 * @param value The value to search for.
	 */
	template <typename Collection, typename Value>
	auto equal_range(const Collection& collection, const Value& value)
	{
		return iterator_range(collection.equal_range(value));
	}

	/** Returns a range representing a reverse iterator for the specified colleciton.
	 * @param collection The collection to create a reverse iterator for.
	 */
	template <typename Collection>
	auto reverse_range(const Collection& collection)
	{
		return iterator_range(collection.rbegin(), collection.rend());
	}
}

/** Represents a range of iterators. */
template <typename Iterator>
class insp::iterator_range final
{
private:
	/** An iterator which points to the start of the range. */
	const Iterator begini;

	/* An iterator which points to one past the end of the range. */
	const Iterator endi;

public:
	/** Initialises a new iterator range with the specified iterators.
	 * @param begin An iterator which points to the start of the range.
	 * @param end An iterator which points to one past the end of the range.
	 */
	explicit iterator_range(Iterator begin, Iterator end)
		: begini(begin)
		, endi(end)
	{
	}

	/** Initialises a new iterator range from a pair of iterators.
	 * @param range A pair of iterators in the format [first, last).
	 */
	explicit iterator_range(std::pair<Iterator, Iterator> range)
		: begini(range.first)
		, endi(range.second)
	{
	}

	/** Determines whether the iterator range is empty. */
	bool empty() const { return begini == endi; }

	/** Retrieves an iterator which points to the start of the range. */
	const Iterator& begin() const { return begini; }

	/** Retrieves an iterator which points to one past the end of the range. */
	const Iterator& end() const { return endi; }

	/** Retrieves the number of hops within the iterator range. */
	typename std::iterator_traits<Iterator>::difference_type count() const { return std::distance(begini, endi); }
};
