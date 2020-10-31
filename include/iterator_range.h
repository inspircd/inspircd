/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Sadie Powell <sadie@witchery.services>
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

namespace stdalgo
{
	template <typename Iterator>
	class iterator_range;
}

template <typename Iterator>
class stdalgo::iterator_range
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
	iterator_range(Iterator begin, Iterator end)
		: begini(begin)
		, endi(end)
	{
	}

	/** Initialises a new iterator range from a pair of iterators.
	 * @param range A pair of iterators in the format [first, last).
	 */
	iterator_range(std::pair<Iterator, Iterator> range)
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
	typename Iterator::difference_type count() const { return std::distance(begini, endi); }
};
