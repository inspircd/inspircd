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

#include <string>
#include "inspircd.h"
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

bool StrHashComp::operator()(const string& s1, const string& s2) const
{
        char a[MAXBUF],b[MAXBUF];
        strlcpy(a,s1.c_str(),MAXBUF);
        strlcpy(b,s2.c_str(),MAXBUF);
        strlower(a);
        strlower(b);
        return (strcasecmp(a,b) == 0);
}

bool InAddr_HashComp::operator()(const in_addr &s1, const in_addr &s2) const
{
        size_t q;
        size_t p;

        memcpy(&q,&s1,sizeof(size_t));
        memcpy(&p,&s2,sizeof(size_t));

        return (q == p);
}

