#ifndef _HASHCOMP_H_
#define _HASHCOMP_H_

#include "inspircd.h"
#include "inspircd_io.h"
#include "inspircd_util.h"
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
                size_t operator()(const struct in_addr &a) const
                {
                        size_t q;
                        memcpy(&q,&a,sizeof(size_t));
                        return q;
                }
        };
#ifdef GCC34
        template<> struct hash<string>
#else
        template<> struct nspace::hash<string>
#endif
        {
                size_t operator()(const string &s) const
                {
                        char a[MAXBUF];
                        static struct hash<const char *> strhash;
                        strlcpy(a,s.c_str(),MAXBUF);
                        strlower(a);
                        return strhash(a);
                }
        };
}


struct StrHashComp
{

        bool operator()(const string& s1, const string& s2) const
        {
                char a[MAXBUF],b[MAXBUF];
                strlcpy(a,s1.c_str(),MAXBUF);
                strlcpy(b,s2.c_str(),MAXBUF);
                strlower(a);
                strlower(b);
                return (strcasecmp(a,b) == 0);
        }

};

struct InAddr_HashComp
{

        bool operator()(const in_addr &s1, const in_addr &s2) const
        {
                size_t q;
                size_t p;

                memcpy(&q,&s1,sizeof(size_t));
                memcpy(&p,&s2,sizeof(size_t));

                return (q == p);
        }

};



#endif
