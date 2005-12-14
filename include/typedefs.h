#ifndef __TYPEDEF_H__
#define __TYPEDEF_H__

#include "users.h"
#include "channels.h"
#include "hashcomp.h"
#include "inspstring.h"
#include "ctables.h"
#include "inspircd.h"
#include "modules.h"
#include "globals.h"
#include "inspircd_config.h"
#include <string>
#ifdef GCC3
#include <ext/hash_map>
#else
#include <hash_map>
#endif

typedef nspace::hash_map<std::string, userrec*, nspace::hash<string>, irc::StrHashComp> user_hash;
typedef nspace::hash_map<std::string, chanrec*, nspace::hash<string>, irc::StrHashComp> chan_hash;
typedef nspace::hash_map<in_addr,string*, nspace::hash<in_addr>, irc::InAddr_HashComp> address_cache;
typedef nspace::hash_map<std::string, WhoWasUser*, nspace::hash<string>, irc::StrHashComp> whowas_hash;
typedef std::deque<command_t> command_table;
typedef std::vector<std::string> servernamelist;
typedef std::vector<ExtMode> ExtModeList;
typedef ExtModeList::iterator ExtModeListIter;
typedef void (handlerfunc) (char**, int, userrec*);
typedef std::deque<std::string> file_cache;

#endif
