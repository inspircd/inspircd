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

#include "inspircd.h"
#include "hashcomp.h"
#include <ext/hash_map>
#define nspace __gnu_cxx

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

/* convert a string to lowercase. Note following special circumstances
 * taken from RFC 1459. Many "official" server branches still hold to this
 * rule so i will too;
 *
 *  Because of IRC's scandanavian origin, the characters {}| are
 *  considered to be the lower case equivalents of the characters []\,
 *  respectively. This is a critical issue when determining the
 *  equivalence of two nicknames.
 */
void nspace::strlower(char *n)
{
	if (n)
	{
		for (char* t = n; *t; t++)
			*t = lowermap[(unsigned char)*t];
	}
}

size_t nspace::hash<insp_inaddr>::operator()(const insp_inaddr &a) const
{
	size_t q;
	memcpy(&q,&a,sizeof(size_t));
	return q;
}

size_t nspace::hash<string>::operator()(const string &s) const
{
	/* XXX: NO DATA COPIES! :)
	 * The hash function here is practically
	 * a copy of the one in STL's hash_fun.h,
	 * only with *x replaced with lowermap[*x].
	 * This avoids a copy to use hash<const char*>
	 */
	register size_t t = 0;
	for (std::string::const_iterator x = s.begin(); x != s.end(); x++) /* ++x not x++, so we don't hash the \0 */
		t = 5 * t + lowermap[(unsigned char)*x];
	return size_t(t);
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
 * This class depends on the const array 'lowermap'.
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

			std::string strip(lsp, n+1 == tokens.end() ? n+1  : n++);
			while ((strip.length()) && (strip.find_last_of(' ') == strip.length() - 1))
				strip.erase(strip.end() - 1);

			return strip;
		}

		n++;
	}
	return "";
}

irc::sepstream::sepstream(const std::string &source, char seperator) : tokens(source), sep(seperator)
{
	last_starting_position = tokens.begin();
	n = tokens.begin();
}

const std::string irc::sepstream::GetToken()
{
	std::string::iterator lsp = last_starting_position;

	while (n != tokens.end())
	{
		if ((*n == sep) || (n+1 == tokens.end()))
		{
			last_starting_position = n+1;
			std::string strip = std::string(lsp, n+1 == tokens.end() ? n+1  : n++);

			while ((strip.length()) && (strip.find_last_of(sep) == strip.length() - 1))
				strip.erase(strip.end() - 1);

			return strip;
		}

		n++;
	}

	return "";
}

irc::sepstream::~sepstream()
{
}

std::string irc::hex(const unsigned char *raw, size_t rawsz)
{
	if (!rawsz)
		return "";

	char buf[rawsz*2+1];
	size_t i;

	for (i = 0; i < rawsz; i++)
	{
		sprintf (&(buf[i*2]), "%02x", raw[i]);
	}
	buf[i*2] = 0;

	return buf;
}

const char* irc::Spacify(char* n)
{
	static char x[MAXBUF];
	strlcpy(x,n,MAXBUF);
	for (char* y = x; *y; y++)
		if (*y == '_')
			*y = ' ';
	return x;
}


irc::modestacker::modestacker(bool add) : adding(add)
{
	sequence.clear();
	sequence.push_back("");
}

void irc::modestacker::Push(char modeletter, const std::string &parameter)
{
	*(sequence.begin()) += modeletter;
	sequence.push_back(parameter);
}

int irc::modestacker::GetStackedLine(std::deque<std::string> &result)
{
	result.clear();
	result.push_back(adding ? "+" : "-");

	while (!sequence[0].empty() && (sequence.size() > 1) && (result.size() < MAXMODES+1))
	{
		result[0] += *(sequence[0].begin());
		result.push_back(sequence[1]);
		sequence[0].erase(sequence[0].begin());
		sequence.erase(sequence.begin() + 1);
	}

	return result.size();
}

