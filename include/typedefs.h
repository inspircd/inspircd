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
#include <ext/hash_map>

typedef nspace::hash_map<std::string, userrec*, nspace::hash<string>, irc::StrHashComp> user_hash;
typedef nspace::hash_map<std::string, chanrec*, nspace::hash<string>, irc::StrHashComp> chan_hash;

typedef std::vector<std::string> servernamelist;
typedef std::vector<ExtMode> ExtModeList;
typedef ExtModeList::iterator ExtModeListIter;
typedef std::deque<std::string> file_cache;

#endif
