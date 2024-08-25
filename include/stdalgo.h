/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020-2022 Sadie Powell <sadie@witchery.services>
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
		 * Returns nothing, but all iterators, references and pointers to the erased element and the
		 * last element are invalidated
		 * @param vect Vector to remove the element from
		 * @param it Iterator to the element to remove
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

	/**
	 * Deletes all elements in a container using operator delete
	 * @param cont The container containing the elements to delete
	 */
	template <template<typename, typename> class Cont, typename T, typename Alloc>
	inline void delete_all(const Cont<T*, Alloc>& cont)
	{
		std::for_each(cont.begin(), cont.end(), std::default_delete<T>());
	}

	/** Deletes a object and zeroes the memory location that pointed to it.
	 * @param pr A reference to the pointer that contains the object to delete.
	 */
	template<typename T>
	void delete_zero(T*& pr)
	{
		auto p = pr;
		pr = nullptr;
		delete p;
	}

	/**
	 * Check if an element with the given value is in a container. Equivalent to (std::find(cont.begin(), cont.end(), val) != cont.end()).
	 * @param cont Container to find the element in
	 * @param val Value of the element to look for
	 * @return True if the element was found in the container, false otherwise
	 */
	template <template<typename, typename> class Cont, typename T, typename Alloc>
	inline bool isin(const Cont<T, Alloc>& cont, const T& val)
	{
		return (std::find(cont.begin(), cont.end(), val) != cont.end());
	}
}
