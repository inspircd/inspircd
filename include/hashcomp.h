/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2011 Adam <Adam@anope.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2009 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
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

#include <cstring>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include "inspircd.h"

/*******************************************************
 * This file contains classes and templates that deal
 * with the comparison and hashing of 'irc strings'.
 * An 'irc string' is a string which compares in a
 * case insensitive manner, and as per RFC 1459 will
 * treat [ identical to {, ] identical to }, and \
 * as identical to |.
 *
 * There are functors that accept std::string and
 * compare/hash them as type irc::string by using
 * mapping arrays internally.
 *******************************************************/

/** Seperate from the other casemap tables so that code *can* still exclusively rely on RFC casemapping
 * if it must.
 *
 * This is provided as a pointer so that modules can change it to their custom mapping tables,
 * e.g. for national character support.
 */
CoreExport extern unsigned const char *national_case_insensitive_map;

/** A mapping of uppercase to lowercase, including scandinavian
 * 'oddities' as specified by RFC1459, e.g. { -> [, and | -> \
 */
CoreExport extern unsigned const char rfc_case_insensitive_map[256];

/** Case insensitive map, ASCII rules.
 * That is;
 * [ != {, but A == a.
 */
CoreExport extern unsigned const char ascii_case_insensitive_map[256];

/** Case sensitive (identity) map.
 */
CoreExport extern unsigned const char rfc_case_sensitive_map[256];

/** The irc namespace contains a number of helper classes.
 */
namespace irc
{
	/** Check if two IRC object (e.g. nick or channel) names are equal.
	 * This function uses national_case_insensitive_map to determine equality, which, by default does comparison
	 * according to RFC 1459, treating certain otherwise non-identical characters as identical.
	 * @param s1 First string to compare
	 * @param s2 Second string to compare
	 * @return True if the two names are equal, false otherwise
	 */
	CoreExport bool equals(const std::string& s1, const std::string& s2);

	/** This class returns true if two strings match.
	 * Case sensitivity is ignored, and the RFC 'character set'
	 * is adhered to
	 */
	struct StrHashComp
	{
		/** The operator () does the actual comparison in hash_map
		 */
		bool operator()(const std::string& s1, const std::string& s2) const
		{
			return equals(s1, s2);
		}
	};

	struct insensitive
	{
		size_t CoreExport operator()(const std::string &s) const;
	};

	struct insensitive_swo
	{
		bool CoreExport operator()(const std::string& a, const std::string& b) const;
	};

	/** The irc_char_traits class is used for RFC-style comparison of strings.
	 * This class is used to implement irc::string, a case-insensitive, RFC-
	 * comparing string class.
	 */
	struct CoreExport irc_char_traits : public std::char_traits<char>
	{
		/** Check if two chars match.
		 * @param c1st First character
		 * @param c2nd Second character
		 * @return true if the characters are equal
		 */
		static bool eq(char c1st, char c2nd);

		/** Check if two chars do NOT match.
		 * @param c1st First character
		 * @param c2nd Second character
		 * @return true if the characters are unequal
		 */
		static bool ne(char c1st, char c2nd);

		/** Check if one char is less than another.
		 * @param c1st First character
		 * @param c2nd Second character
		 * @return true if c1st is less than c2nd
		 */
		static bool lt(char c1st, char c2nd);

		/** Compare two strings of size n.
		 * @param str1 First string
		 * @param str2 Second string
		 * @param n Length to compare to
		 * @return similar to strcmp, zero for equal, less than zero for str1
		 * being less and greater than zero for str1 being greater than str2.
		 */
		static int compare(const char* str1, const char* str2, size_t n);

		/** Find a char within a string up to position n.
		 * @param s1 String to find in
		 * @param n Position to search up to
		 * @param c Character to search for
		 * @return Pointer to the first occurance of c in s1
		 */
		static const char* find(const char* s1, int  n, char c);
	};

	/** This typedef declares irc::string based upon irc_char_traits.
	 */
	typedef std::basic_string<char, irc_char_traits, std::allocator<char> > string;

	/** Joins the contents of a vector to a string.
	 * @param sequence Zero or more items to join.
	 * @param separator The character to place between the items, defaults to ' ' (space).
	 * @return Joined string.
	 */
   	std::string CoreExport stringjoiner(const std::vector<std::string>& sequence, char separator = ' ');

	/** irc::sepstream allows for splitting token seperated lists.
	 * Each successive call to sepstream::GetToken() returns
	 * the next token, until none remain, at which point the method returns
	 * false.
	 */
	class CoreExport sepstream
	{
	 protected:
		/** Original string.
		 */
		std::string tokens;
		/** Separator value
		 */
		char sep;
		/** Current string position
		 */
		size_t pos;
		/** If set then GetToken() can return an empty string
		 */
		bool allow_empty;
	 public:
		/** Create a sepstream and fill it with the provided data
		 */
		sepstream(const std::string &source, char separator, bool allowempty = false);

		/** Fetch the next token from the stream
		 * @param token The next token from the stream is placed here
		 * @return True if tokens still remain, false if there are none left
		 */
		bool GetToken(std::string& token);

		/** Fetch the entire remaining stream, without tokenizing
		 * @return The remaining part of the stream
		 */
		const std::string GetRemaining();

		/** Returns true if the end of the stream has been reached
		 * @return True if the end of the stream has been reached, otherwise false
		 */
		bool StreamEnd();
	};

	/** A derived form of sepstream, which seperates on commas
	 */
	class CoreExport commasepstream : public sepstream
	{
	 public:
		/** Initialize with comma separator
		 */
		commasepstream(const std::string &source, bool allowempty = false) : sepstream(source, ',', allowempty)
		{
		}
	};

	/** A derived form of sepstream, which seperates on spaces
	 */
	class CoreExport spacesepstream : public sepstream
	{
	 public:
		/** Initialize with space separator
		 */
		spacesepstream(const std::string &source, bool allowempty = false) : sepstream(source, ' ', allowempty)
		{
		}
	};

	/** irc::tokenstream reads a string formatted as per RFC1459 and RFC2812.
	 * It will split the string into 'tokens' each containing one parameter
	 * from the string.
	 * For instance, if it is instantiated with the string:
	 * "PRIVMSG #test :foo bar baz qux"
	 * then each successive call to tokenstream::GetToken() will return
	 * "PRIVMSG", "#test", "foo bar baz qux", "".
	 * Note that if the whole string starts with a colon this is not taken
	 * to mean the string is all one parameter, and the first item in the
	 * list will be ":item". This is to allow for parsing 'source' fields
	 * from data.
	 */
	class CoreExport tokenstream : private spacesepstream
	{
	 public:
		/** Create a tokenstream and fill it with the provided data
		 */
		tokenstream(const std::string &source);

		/** Fetch the next token from the stream as a std::string
		 * @param token The next token available, or an empty string if none remain
		 * @return True if tokens are left to be read, false if the last token was just retrieved.
		 */
		bool GetToken(std::string &token);

		/** Fetch the next token from the stream as an integer
		 * @param token The next token available, or undefined if none remain
		 * @return True if tokens are left to be read, false if the last token was just retrieved.
		 */
		bool GetToken(int &token);

		/** Fetch the next token from the stream as a long integer
		 * @param token The next token available, or undefined if none remain
		 * @return True if tokens are left to be read, false if the last token was just retrieved.
		 */
		bool GetToken(long &token);
	};

	/** The portparser class seperates out a port range into integers.
	 * A port range may be specified in the input string in the form
	 * "6660,6661,6662-6669,7020". The end of the stream is indicated by
	 * a return value of 0 from portparser::GetToken(). If you attempt
	 * to specify an illegal range (e.g. one where start >= end, or
	 * start or end < 0) then GetToken() will return the first element
	 * of the pair of numbers.
	 */
	class CoreExport portparser
	{
	 private:

		/** Used to split on commas
		 */
		commasepstream sep;

		/** Current position in a range of ports
		 */
		long in_range;

		/** Starting port in a range of ports
		 */
		long range_begin;

		/** Ending port in a range of ports
		 */
		long range_end;

		/** Allow overlapped port ranges
		 */
		bool overlapped;

		/** Used to determine overlapping of ports
		 * without O(n) algorithm being used
		 */
		std::set<long> overlap_set;

		/** Returns true if val overlaps an existing range
		 */
		bool Overlaps(long val);
	 public:

		/** Create a portparser and fill it with the provided data
		 * @param source The source text to parse from
		 * @param allow_overlapped Allow overlapped ranges
		 */
		portparser(const std::string &source, bool allow_overlapped = true);

		/** Fetch the next token from the stream
		 * @return The next port number is returned, or 0 if none remain
		 */
		long GetToken();
	};
}
