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

using namespace std;

#ifdef GCC34
size_t hash<in_addr>::operator()(const struct in_addr &a) const
#else
size_t nspace::hash<in_addr>::operator()(const struct in_addr &a) const
#endif
{
        size_t q;
        memcpy(&q,&a,sizeof(size_t));
        return q;
}

#ifdef GCC34
size_t hash<string>::operator()(const string &s) const
#else
size_t nspace::hash<string>::operator()(const string &s) const
#endif
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

