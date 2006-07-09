/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *		       E-mail:
 *		<brain@chatspike.net>
 *		<Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include "inspircd_config.h"
#include "inspircd.h"
#include <string>
#include "hashcomp.h"
#include "helperfuncs.h"
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

// from helperfuncs.cpp
extern const char lowermap[255];

/******************************************************
 *
 * The hash functions of InspIRCd are the centrepoint
 * of the entire system. If these functions are
 * inefficient or wasteful, the whole program suffers
 * as a result. A lot of C programmers in the ircd
 * scene spend a lot of time debating (arguing) about
 * the best way to write hash functions to hash irc
 * nicknames, channels etc.
 * We are lucky as C++ developers as hash_map does
 * a lot of this for us. It does intellegent memory
 * requests, bucketing, search functions, insertion
 * and deletion etc. All we have to do is write some
 * overloaded comparison and hash value operators which
 * cause it to act in an irc-like way. The features we
 * add to the standard hash_map are:
 *
 * Case insensitivity: The hash_map will be case
 * insensitive.
 *
 * Scandanavian Comparisons: The characters [, ], \ will
 * be considered the lowercase of {, } and |.
 *
 * This file also contains hashing methods for hashing
 * in_addr structs, we use this if we want to cache IP
 * addresses.
 *
 ******************************************************/

using namespace std;

size_t nspace::hash<in_addr>::operator()(const struct in_addr &a) const
{
	size_t q;
	memcpy(&q,&a,sizeof(size_t));
	return q;
}

size_t nspace::hash<string>::operator()(const string &s) const
{
	char a[MAXBUF];
	static struct hash<const char *> strhash;
	strlcpy(a,s.c_str(),MAXBUF);
	strlower(a);
	return strhash(a);
}

bool irc::StrHashComp::operator()(const std::string& s1, const std::string& s2) const
{
	unsigned char* n1 = (unsigned char*)s1.c_str();
	unsigned char* n2 = (unsigned char*)s2.c_str();
	for (; *n1 && *n2; n1++, n2++)
		if (lowermap[*n1] != lowermap[*n2])
			return false;
	return (lowermap[*n1] == lowermap[*n2]);
}

bool irc::InAddr_HashComp::operator()(const in_addr &s1, const in_addr &s2) const
{
	return (s1.s_addr == s1.s_addr);
}

/******************************************************
 *
 * This is the implementation of our special irc::string
 * class which is a case-insensitive equivalent to
 * std::string which is not only case-insensitive but
 * can also do scandanavian comparisons, e.g. { = [, etc.
 *
 * This class depends on the global 'lowermap' which is
 * initialized at startup by inspircd.cpp, and contains
 * the 'scandanavian' casemappings for fast irc compare.
 *
 ******************************************************/

bool irc::irc_char_traits::eq(char c1st, char c2nd)
{
	return lowermap[(unsigned char)c1st] == lowermap[(unsigned char)c2nd];
}

bool irc::irc_char_traits::ne(char c1st, char c2nd)
{
	return lowermap[(unsigned char)c1st] != lowermap[(unsigned char)c2nd];
}

bool irc::irc_char_traits::lt(char c1st, char c2nd)
{
	return lowermap[(unsigned char)c1st] < lowermap[(unsigned char)c2nd];
}

int irc::irc_char_traits::compare(const char* str1, const char* str2, size_t n)
{
	for(unsigned int i = 0; i < n; i++)
	{
		if(lowermap[(unsigned char)*str1] > lowermap[(unsigned char)*str2])
       			return 1;

		if(lowermap[(unsigned char)*str1] < lowermap[(unsigned char)*str2])
		       	return -1;

		if(*str1 == 0 || *str2 == 0)
		      	return 0;

	       	str1++;
		str2++;
	}
	return 0;
}

std::string operator+ (std::string& leftval, irc::string& rightval)
{
	return leftval + std::string(rightval.c_str());
}

irc::string operator+ (irc::string& leftval, std::string& rightval)
{
	return leftval + irc::string(rightval.c_str());
}

bool operator== (std::string& leftval, irc::string& rightval)
{
	return (leftval == std::string(rightval.c_str()));
}

bool operator== (irc::string& leftval, std::string& rightval)
{
	return (rightval == std::string(leftval.c_str()));
}

const char* irc::irc_char_traits::find(const char* s1, int  n, char c)
{
	while(n-- > 0 && lowermap[(unsigned char)*s1] != lowermap[(unsigned char)c])
		s1++;
	return s1;
}

/* See hashcomp.h if you care about these... */
std::ostream& operator<<(std::ostream &os, const irc::string &str)
{
	return os << str.c_str();
}

std::istream& operator>>(std::istream &is, irc::string &str)
{
	std::string tmp;
	is >> tmp;
	str = tmp.c_str();
	return is;
}
