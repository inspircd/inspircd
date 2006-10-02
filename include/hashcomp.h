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
using irc::sockets::insp_aton;
using irc::sockets::insp_ntoa;
using irc::sockets::insp_inaddr;

#ifndef LOWERMAP
#define LOWERMAP
/** A mapping of uppercase to lowercase, including scandinavian
 * 'oddities' as specified by RFC1459, e.g. { -> [, and | -> \
 */
unsigned const char lowermap[256] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,				/* 0-19 */
				20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,				/* 20-39 */
				40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,				/* 40-59 */
				60, 61, 62, 63, 64, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,		/* 60-79 */
				112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 94, 95, 96, 97, 98, 99,		/* 80-99 */
				100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,	/* 100-119 */
				120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139,	/* 120-139 */
				140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,	/* 140-159 */
				160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179,	/* 160-179 */
				180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199,	/* 180-199 */
				200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219,	/* 200-219 */
				220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,	/* 220-239 */
				240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255				/* 240-255 */
};
#endif

/** Because of weirdness in g++, before 3.x this was namespace std. It's now __gnu_cxx.
 * This is a #define'd alias.
 */
namespace nspace
{
	/** Convert a string to lower case respecting RFC1459
	 * @param n A string to lowercase
	 */
	void strlower(char *n);

	/** Hashing function to hash insp_inaddr structs
	 */
        template<> struct hash<insp_inaddr>
        {
		/** Hash an insp_inaddr
		 * @param a An insp_inaddr to hash
		 * @return The hash value
		 */
                size_t operator()(const insp_inaddr &a) const;
        };

	/** Hashing function to hash std::string without respect to case
	 */
        template<> struct hash<std::string>
        {
		/** Hash a std::string using RFC1459 case sensitivity rules
		 * @param s A string to hash
		 * @return The hash value
		 */
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


	/** This class returns true if two insp_inaddr structs match.
	 * Checking is done by copying both into a size_t then doing a
	 * numeric comparison of the two.
	 */
	struct InAddr_HashComp
	{
		/** The operator () does the actual comparison in hash_map
		 */
	        bool operator()(const insp_inaddr &s1, const insp_inaddr &s2) const;
	};

	/** irc::modestacker stacks mode sequences into a list.
	 * It can then reproduce this list, clamped to a maximum of MAXMODES
	 * values per line.
	 */
	class modestacker
	{
	 private:
		/** The mode sequence and its parameters
		 */
		std::deque<std::string> sequence;
		/** True if the mode sequence is initially adding
		 * characters, false if it is initially removing
		 * them
		 */
		bool adding;
	 public:
		/** Construct a new modestacker.
		 * @param add True if the stack is adding modes,
		 * false if it is removing them
		 */
		modestacker(bool add);
		/** Push a modeletter and its parameter onto the stack.
		 * No checking is performed as to if this mode actually
		 * requires a parameter. If you stack invalid mode
		 * sequences, they will be tidied if and when they are
		 * passed to a mode parser.
		 * @param modeletter The mode letter to insert
		 * @param parameter The parameter for the mode
		 */
		void Push(char modeletter, const std::string &parameter);
		/** Push a modeletter without parameter onto the stack.
		 * No checking is performed as to if this mode actually
		 * requires a parameter. If you stack invalid mode
		 * sequences, they will be tidied if and when they are
		 * passed to a mode parser.
		 * @param modeletter The mode letter to insert
		 */
		void Push(char modeletter);
		/** Push a '+' symbol onto the stack.
		 */
		void PushPlus();
		/** Push a '-' symbol onto the stack.
		 */
		void PushMinus();
		/** Return zero or more elements which form the
		 * mode line. This will be clamped to a max of
		 * MAXMODES+1 items (MAXMODES mode parameters and
		 * one mode dequence string).
		 * @param result The deque to populate. This will
		 * be cleared before it is used.
		 * @return The number of elements in the deque
		 */
		int GetStackedLine(std::deque<std::string> &result);
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
	class tokenstream
	{
	 private:
		std::string tokens;
		std::string::iterator last_starting_position;
		std::string::iterator n;
		bool last_pushed;
	 public:
		/** Create a tokenstream and fill it with the provided data
		 */
		tokenstream(const std::string &source);
		~tokenstream();

		/** Fetch the next token from the stream
		 * @returns The next token is returned, or an empty string if none remain
		 */
		const std::string GetToken();
	};

	/** irc::commasepstream allows for splitting comma seperated lists.
	 * Lists passed to irc::commasepstream should not contain spaces
	 * after the commas, or this will be taken to be part of the item
	 * data. Each successive call to commasepstream::GetToken() returns
	 * the next token, until none remain, at which point the method returns
	 * an empty string.
	 */
	class sepstream : public classbase
	{
	 private:
		std::string tokens;
		std::string::iterator last_starting_position;
		std::string::iterator n;
		char sep;
	 public:
		/** Create a commasepstream and fill it with the provided data
		 */
		sepstream(const std::string &source, char seperator);
		virtual ~sepstream();

		/** Fetch the next token from the stream
		 * @returns The next token is returned, or an empty string if none remain
		 */
		virtual const std::string GetToken();
	};

	class commasepstream : public sepstream
	{
	 public:
		commasepstream(const std::string &source) : sepstream(source, ',')
		{
		}
	};

	class spacesepstream : public sepstream
	{
	 public:
		spacesepstream(const std::string &source) : sepstream(source, ' ')
		{
		}
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

	std::string hex(const unsigned char *raw, size_t rawsz);

	/** This typedef declares irc::string based upon irc_char_traits
	 */
	typedef basic_string<char, irc_char_traits, allocator<char> > string;

	const char* Spacify(char* n);
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
