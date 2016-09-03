/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2005, 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#pragma once

class BanCacheManager;
class BufferedSocket;
class Channel;
class Command;
class ConfigStatus;
class ConfigTag;
class Extensible;
class FakeUser;
class InspIRCd;
class Invitation;
class IOHookProvider;
class LocalUser;
class Membership;
class Module;
class OperInfo;
class ProtocolServer;
class RemoteUser;
class Server;
class ServerConfig;
class ServerLimits;
class Thread;
class User;
class XLine;
class XLineManager;
class XLineFactory;
struct ConnectClass;
struct ModResult;

#include "hashcomp.h"
#include "base.h"

typedef TR1NS::unordered_map<std::string, User*, irc::insensitive, irc::StrHashComp> user_hash;
typedef TR1NS::unordered_map<std::string, Channel*, irc::insensitive, irc::StrHashComp> chan_hash;

/** A list of failed port bindings, used for informational purposes on startup */
typedef std::vector<std::pair<std::string, std::string> > FailedPortList;

/** List of channels to consider when building the neighbor list of a user
 */
typedef std::vector<Membership*> IncludeChanList;

/** A cached text file stored with its contents as lines
 */
typedef std::vector<std::string> file_cache;

/** A configuration key and value pair
 */
typedef std::pair<std::string, std::string> KeyVal;

/** The entire configuration
 */
typedef std::multimap<std::string, reference<ConfigTag> > ConfigDataHash;

/** Iterator of ConfigDataHash */
typedef ConfigDataHash::const_iterator ConfigIter;
/** Iterator pair, used for tag-name ranges */
typedef std::pair<ConfigIter,ConfigIter> ConfigTagList;

/** Files read by the configuration */
typedef std::map<std::string, file_cache> ConfigFileCache;

/** Generic user list, used for exceptions */
typedef std::set<User*> CUList;

/** A set of strings.
 */
typedef std::vector<std::string> string_list;

/** Contains an ident and host split into two strings
 */
typedef std::pair<std::string, std::string> IdentHostPair;

/** A map of xline factories
 */
typedef std::map<std::string, XLineFactory*> XLineFactMap;

/** A map of XLines indexed by string
 */
typedef std::map<std::string, XLine*, irc::insensitive_swo> XLineLookup;

/** A map of XLineLookup maps indexed by string
 */
typedef std::map<std::string, XLineLookup > XLineContainer;

/** An iterator in an XLineContainer
 */
typedef XLineContainer::iterator ContainerIter;

/** An interator in an XLineLookup
 */
typedef XLineLookup::iterator LookupIter;

namespace Stats
{
	class Context;
}
