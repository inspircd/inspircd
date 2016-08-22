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

	namespace string
	{
		/** Get underlying C string of the string passed as parameter. Useful in template functions.
		 * @param str C string
		 * @return Same as input
		 */
		inline const char* tocstr(const char* str)
		{
			return str;
		}

		/** Get underlying C string of the string passed as parameter. Useful in template functions.
		 * @param str std::string object
		 * @return str.c_str()
		 */
		inline const char* tocstr(const std::string& str)
		{
			return str.c_str();
		}

		/** Check if two strings are equal case insensitively.
		 * @param str1 First string to compare.
		 * @param str2 Second string to compare.
		 * @return True if the strings are equal case-insensitively, false otherwise.
		 */
		template <typename S1, typename S2>
		inline bool equalsci(const S1& str1, const S2& str2)
		{
			return (!strcasecmp(tocstr(str1), tocstr(str2)));
		}

		/** Replace first occurrence of a substring ('target') in a string ('str') with another string ('replacement').
		 * @param str String to perform replacement in
		 * @param target String to replace
		 * @param replacement String to put in place of 'target'
		 * @return True if 'target' was replaced with 'replacement', false if it was not found in 'str'.
		 */
		template<typename CharT, typename Traits, typename Alloc>
		inline bool replace(std::basic_string<CharT, Traits, Alloc>& str, const std::basic_string<CharT, Traits, Alloc>& target, const std::basic_string<CharT, Traits, Alloc>& replacement)
		{
			const typename std::basic_string<CharT, Traits, Alloc>::size_type p = str.find(target);
			if (p == std::basic_string<CharT, Traits, Alloc>::npos)
				return false;
			str.replace(p, target.size(), replacement);
			return true;
		}

		/** Replace all occurrences of a string ('target') in a string ('str') with another string ('replacement').
		 * @param str String to perform replacement in
		 * @param target String to replace
		 * @param replacement String to put in place of 'target'
		 */
		template<typename CharT, typename Traits, typename Alloc>
		inline void replace_all(std::basic_string<CharT, Traits, Alloc>& str, const std::basic_string<CharT, Traits, Alloc>& target, const std::basic_string<CharT, Traits, Alloc>& replacement)
		{
			if (target.empty())
				return;

			typename std::basic_string<CharT, Traits, Alloc>::size_type p = 0;
			while ((p = str.find(target, p)) != std::basic_string<CharT, Traits, Alloc>::npos)
			{
				str.replace(p, target.size(), replacement);
				p += replacement.size();
			}
		}
	}

	/**
	 * Deleter that uses operator delete to delete the item
	 */
	template <typename T>
	struct defaultdeleter
	{
		void operator()(T* o)
		{
			delete o;
		}
	};

	/**
	 * Deleter that adds the item to the cull list, that is, queues it for
	 * deletion at the end of the current mainloop iteration
	 */
	struct culldeleter
	{
		void operator()(classbase* item);
	};

	/**
	 * Deletes all elements in a container using operator delete
	 * @param cont The container containing the elements to delete
	 */
	template <template<typename, typename> class Cont, typename T, typename Alloc>
	inline void delete_all(const Cont<T*, Alloc>& cont)
	{
		std::for_each(cont.begin(), cont.end(), defaultdeleter<T>());
	}

	/**
	 * Remove an element from a container
	 * @param cont Container to remove the element from
	 * @param val Value of the element to look for and remove
	 * @return True if the element was found and removed, false otherwise
	 */
	template <template<typename, typename> class Cont, typename T, typename Alloc>
	inline bool erase(Cont<T, Alloc>& cont, const T& val)
	{
		const typename Cont<T, Alloc>::iterator it = std::find(cont.begin(), cont.end(), val);
		if (it != cont.end())
		{
			cont.erase(it);
			return true;
		}
		return false;
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
