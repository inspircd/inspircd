/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
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
#include "socket.h"
#include "hash_map.h"

/*******************************************************
 * This file contains classes and templates that deal
 * with the comparison and hashing of 'irc strings'.
 * An 'irc string' is a string which compares in a
 * case insensitive manner, and as per RFC 1459 will
 * treat [ identical to {, ] identical to }, and \
 * as identical to |.
 *
 * Our hashing functions are designed  to accept
 * std::string and compare/hash them as type irc::string
 * by converting them internally. This makes them
 * backwards compatible with other code which is not
 * aware of irc::string.
 *******************************************************/
 
using namespace std;

namespace nspace
{
        template<> struct hash<in_addr>
        {
                size_t operator()(const struct in_addr &a) const;
        };

        template<> struct hash<std::string>
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

	class tokenstream
	{
	 private:
		std::string tokens;
		std::string::iterator last_starting_position;
		std::string::iterator n;
		bool last_pushed;
	 public:
		tokenstream(const std::string &source);
		~tokenstream();

		const std::string GetToken();
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

/* Define operators for using >> and << with irc::string to an ostream on an istream. */
/* This was endless fun. No. Really. */
/* It was also the first core change Ommeh made, if anyone cares */

std::ostream& operator<<(std::ostream &os, const irc::string &str);
std::istream& operator>>(std::istream &is, irc::string &str);

/* Define operators for + and == with irc::string to std::string for easy assignment
 * and comparison - Brain
 */

std::string operator+ (std::string& leftval, irc::string& rightval);
irc::string operator+ (irc::string& leftval, std::string& rightval);
bool operator== (std::string& leftval, irc::string& rightval);
bool operator== (irc::string& leftval, std::string& rightval);

#endif
