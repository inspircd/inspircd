/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __TYPEDEF_H__
#define __TYPEDEF_H__

class BanCacheManager;
class BufferedSocket;
class Channel;
class Command;
class ConfigReader;
class ConfigTag;
class DNSHeader;
class DNSRequest;
class Extensible;
class FakeUser;
class InspIRCd;
class ListItem;
class LocalUser;
class Membership;
class ModeID;
class Module;
class OperInfo;
class RemoteUser;
class ServerConfig;
class ServerLimits;
class Thread;
class User;
class UserResolver;
class XLine;
class XLineManager;
class XLineFactory;
struct ConnectClass;
struct ModResult;
struct ResourceRecord;

#include "hashcomp.h"
#include "base.h"

#if defined(WINDOWS) && !defined(HASHMAP_DEPRECATED)
	typedef nspace::hash_map<std::string, User*, nspace::hash_compare<std::string, std::less<std::string> > > user_hash;
	typedef nspace::hash_map<std::string, Channel*, nspace::hash_compare<std::string, std::less<std::string> > > chan_hash;
#else
	#ifdef HASHMAP_DEPRECATED
		typedef nspace::hash_map<std::string, User*, nspace::insensitive, irc::StrHashComp> user_hash;
		typedef nspace::hash_map<std::string, Channel*, nspace::insensitive, irc::StrHashComp> chan_hash;
	#else
		typedef nspace::hash_map<std::string, User*, nspace::hash<std::string>, irc::StrHashComp> user_hash;
		typedef nspace::hash_map<std::string, Channel*, nspace::hash<std::string>, irc::StrHashComp> chan_hash;
	#endif
#endif

/** A list of failed port bindings, used for informational purposes on startup */
typedef std::vector<std::pair<std::string, std::string> > FailedPortList;

/** Holds a complete list of all channels to which a user has been invited and has not yet joined, and the time at which they'll expire.
 */
typedef std::vector< std::pair<irc::string, time_t> > InvitedList;

/** Holds a complete list of all allow and deny tags from the configuration file (connection classes)
 */
typedef std::vector<reference<ConnectClass> > ClassVector;

/** Typedef for the list of user-channel records for a user
 */
typedef std::set<Channel*> UserChanList;

/** Shorthand for an iterator into a UserChanList
 */
typedef UserChanList::iterator UCListIter;

/** A list of custom modes parameters on a channel
 */
typedef std::map<ModeID,std::string> CustomModeList;

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

/** Index of valid oper blocks and types */
typedef std::map<std::string, reference<OperInfo> > OperIndex;

/** Files read by the configuration */
typedef std::map<std::string, file_cache> ConfigFileCache;

/** A hash of commands used by the core
 */
typedef nspace::hash_map<std::string,Command*> Commandtable;

/** Membership list of a channel */
typedef std::map<User*, Membership*> UserMembList;
/** Iterator of UserMembList */
typedef UserMembList::iterator UserMembIter;
/** const Iterator of UserMembList */
typedef UserMembList::const_iterator UserMembCIter;

/** Generic user list, used for exceptions */
typedef std::set<User*> CUList;

/** A set of strings.
 */
typedef std::vector<std::string> string_list;

/** Contains an ident and host split into two strings
 */
typedef std::pair<std::string, std::string> IdentHostPair;

/** Items stored in the channel's list
 */
typedef std::list<ListItem> modelist;

/** A map of xline factories
 */
typedef std::map<std::string, XLineFactory*> XLineFactMap;

/** A map of XLines indexed by string
 */
typedef std::map<irc::string, XLine *> XLineLookup;

/** A map of XLineLookup maps indexed by string
 */
typedef std::map<std::string, XLineLookup > XLineContainer;

/** An iterator in an XLineContainer
 */
typedef XLineContainer::iterator ContainerIter;

/** An interator in an XLineLookup
 */
typedef XLineLookup::iterator LookupIter;

#endif

