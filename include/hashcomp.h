/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2005 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef _HASHCOMP_H_
#define _HASHCOMP_H_

#include "inspircd_config.h"

/**
 * This file contains classes and templates that deal
 * with the comparison and hashing of 'irc strings'.
 * An 'irc string' is a string which compares in a
 * case insensitive manner, and as per RFC 1459 will
 * treat [ identical to {, ] identical to }, and \
 * as identical to |. Our hashing functions are designed
 * to accept std::string and compare/hash them in an irc
 * type way, irc::string is a seperate class type currently.
 */

#ifdef GCC3
#include <ext/hash_map>
#else
#include <hash_map>
#endif

#ifdef GCC3
#define nspace __gnu_cxx
#else
#define nspace std
#endif

using namespace std;

namespace nspace
{
#ifdef GCC34
        template<> struct hash<in_addr>
#else
        template<> struct nspace::hash<in_addr>
#endif
        {
                size_t operator()(const struct in_addr &a) const;
        };
#ifdef GCC34
        template<> struct hash<string>
#else
        template<> struct nspace::hash<string>
#endif
        {
                size_t operator()(const string &s) const;
        };
}

/** The irc namespace contains a number of helper classes.
 */
namespace irc
{

	/** This class returns true if two strings match.
	 * Case sensitivity is ignored, and the RFC 'character set'
	 * is adhered to
	 */
	struct StrHashComp
	{
		/** The operator () does the actual comparison in hash_map
		 */
	        bool operator()(const std::string& s1, const std::string& s2) const;
	};


	/** This class returns true if two in_addr structs match.
	 * Checking is done by copying both into a size_t then doing a
	 * numeric comparison of the two.
	 */
	struct InAddr_HashComp
	{
		/** The operator () does the actual comparison in hash_map
		 */
	        bool operator()(const in_addr &s1, const in_addr &s2) const;
	};


	/** The irc_char_traits class is used for RFC-style comparison of strings.
	 * This class is used to implement irc::string, a case-insensitive, RFC-
	 * comparing string class.
	 */
	struct irc_char_traits : std::char_traits<char> {

		/** Check if two chars match
		 */
		static bool eq(char c1st, char c2nd);

		/** Check if two chars do NOT match
		 */
		static bool ne(char c1st, char c2nd);

		/** Check if one char is less than another
		 */
		static bool lt(char c1st, char c2nd);

		/** Compare two strings of size n
		 */
		static int compare(const char* str1, const char* str2, size_t n);

		/** Find a char within a string up to position n
 		 */
		static const char* find(const char* s1, int  n, char c);
	};

	/** This typedef declares irc::string based upon irc_char_traits
	 */
	typedef basic_string<char, irc_char_traits, allocator<char> > string;
}

#endif
