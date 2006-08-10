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

using namespace std;

#include "inspircd_config.h"
#include "inspircd.h"
#include <string>
#include "hashcomp.h"
#include "helperfuncs.h"
#include <ext/hash_map>

#define nspace __gnu_cxx

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
 * insp_inaddr structs, we use this if we want to cache IP
 * addresses.
 *
 ******************************************************/

using namespace std;
using namespace irc::sockets;

size_t nspace::hash<insp_inaddr>::operator()(const insp_inaddr &a) const
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

bool irc::InAddr_HashComp::operator()(const insp_inaddr &s1, const insp_inaddr &s2) const
{
#ifdef IPV6
	for (int n = 0; n < 16; n++)
		if (s2.s6_addr[n] != s1.s6_addr[n])
			return false;
	return true;
#else
	return (s1.s_addr == s1.s_addr);
#endif
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

irc::tokenstream::tokenstream(const std::string &source) : tokens(source), last_pushed(false)
{
	/* Remove trailing spaces, these muck up token parsing */
	while (tokens.find_last_of(' ') == tokens.length() - 1)
		tokens.erase(tokens.end() - 1);

	/* Record starting position and current position */
	last_starting_position = tokens.begin();
	n = tokens.begin();
}

irc::tokenstream::~tokenstream()
{
}

const std::string irc::tokenstream::GetToken()
{
	std::string::iterator lsp = last_starting_position;

	while (n != tokens.end())
	{
		if ((last_pushed) && (*n == ':'))
		{
			/* If we find a token thats not the first and starts with :,
			 * this is the last token on the line
			 */
			std::string::iterator curr = ++n;
			n = tokens.end();
			return std::string(curr, tokens.end());
		}

		last_pushed = false;

		if ((*n == ' ') || (n+1 == tokens.end()))
		{
			/* If we find a space, or end of string, this is the end of a token.
			 */
			last_starting_position = n+1;
			last_pushed = true;
			return std::string(lsp, n+1 == tokens.end() ? n+1  : n++);
		}

		n++;
	}
	return "";
}

irc::commasepstream::commasepstream(const std::string &source) : tokens(source)
{
	last_starting_position = tokens.begin();
	n = tokens.begin();
}

const std::string irc::commasepstream::GetToken()
{
	std::string::iterator lsp = last_starting_position;

	while (n != tokens.end())
	{
		if ((*n == ',') || (n+1 == tokens.end()))
		{
			last_starting_position = n+1;
			return std::string(lsp, n+1 == tokens.end() ? n+1  : n++);
		}

		n++;
	}

	return "";
}

irc::commasepstream::~commasepstream()
{
}
