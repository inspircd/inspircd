/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2014, 2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2016, 2018, 2020-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005 Craig Edwards <brain@inspircd.org>
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
class ConnectClass;
class ModResult;

namespace ClientProtocol {
class Event;
class EventProvider;
class Message;
class MessageTagEvent;
class MessageTagProvider;
class Serializer;

typedef std::vector<Message*> MessageList;
typedef std::vector<std::string> ParamList;
typedef std::string SerializedMessage;

struct CoreExport MessageTagData {
    MessageTagProvider* tagprov;
    std::string value;
    void* provdata;

    MessageTagData(MessageTagProvider* prov, const std::string& val,
                   void* data = NULL);
};

/** Map of message tag values and providers keyed by their name.
 * Sorted in descending order to ensure tag names beginning with symbols (such as '+') come later when iterating
 * the container than tags with a normal name.
 */
typedef insp::flat_map<std::string, MessageTagData, std::greater<std::string> >
TagMap;
}

#include "hashcomp.h"
#include "base.h"

typedef TR1NS::unordered_map<std::string, User*, irc::insensitive, irc::StrHashComp>
user_hash;
typedef TR1NS::unordered_map<std::string, Channel*, irc::insensitive, irc::StrHashComp>
chan_hash;

/** List of channels to consider when building the neighbor list of a user
 */
typedef std::vector<Membership*> IncludeChanList;

/** A cached text file stored with its contents as lines
 */
typedef std::vector<std::string> file_cache;

/** A mapping of configuration keys to their assigned values.
 */
typedef insp::flat_map<std::string, std::string, irc::insensitive_swo>
ConfigItems;

/** The entire configuration
 */
typedef std::multimap<std::string, reference<ConfigTag>, irc::insensitive_swo>
ConfigDataHash;

/** Iterator of ConfigDataHash */
typedef ConfigDataHash::const_iterator ConfigIter;
/** Iterator pair, used for tag-name ranges */
typedef std::pair<ConfigIter,ConfigIter> ConfigTagList;

/** Files read by the configuration */
typedef std::map<std::string, file_cache> ConfigFileCache;

/** Generic user list, used for exceptions */
typedef std::set<User*> CUList;

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

/** An iterator in an XLineLookup
 */
typedef XLineLookup::iterator LookupIter;

namespace Stats {
class Context;
}
