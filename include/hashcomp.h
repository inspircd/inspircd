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

struct StrHashComp
{

        bool operator()(const string& s1, const string& s2) const;
};

struct InAddr_HashComp
{
        bool operator()(const in_addr &s1, const in_addr &s2) const;
};

namespace irc
{
	struct irc_char_traits : std::char_traits<char> {
		  static bool eq(char c1st, char c2nd);
		  static bool ne(char c1st, char c2nd);
		  static bool lt(char c1st, char c2nd);
		  static int compare(const char* str1, const char* str2, size_t n);
		  static const char* find(const char* s1, int  n, char c);
	};

	typedef basic_string<char, irc_char_traits, allocator<char> > string;
}

#endif
