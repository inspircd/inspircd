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



#endif
