/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
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
	namespace vector
	{
		/**
		 * Erase a single element from a vector by overwriting it with a copy of the last element,
		 * which is then removed. This, in contrast to vector::erase(), does not result in all
		 * elements after the erased element being moved.
		 * @param vect Vector to remove the element from
		 * @param it Iterator to the element to remove
		 * @return Nothing, but all iterators, references and pointers to the erased element and the
		 * last element are invalidated
		 */
		template <typename T>
		inline void swaperase(typename std::vector<T>& vect, const typename std::vector<T>::iterator& it)
		{
			*it = vect.back();
			vect.pop_back();
		}

		/**
		 * Find and if exists, erase a single element from a vector by overwriting it with a
		 * copy of the last element, which is then removed. This, in contrast to vector::erase(),
		 * does not result in all elements after the erased element being moved.
		 * If the given value occurs multiple times, the one with the lowest index is removed.
		 * Individual elements are compared to the given value using operator==().
		 * @param vect Vector to remove the element from
		 * @param val Value of the element to look for and remove
		 * @return True if the element was found and removed, false if it wasn't found.
		 * If true, all iterators, references and pointers pointing to either the first element that
		 * is equal to val or to the last element are invalidated.
		 */
		template <typename T>
		inline bool swaperase(typename std::vector<T>& vect, const T& val)
		{
			const typename std::vector<T>::iterator it = std::find(vect.begin(), vect.end(), val);
			if (it != vect.end())
			{
				swaperase(vect, it);
				return true;
			}
			return false;
		}
	}
}
