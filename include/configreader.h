/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
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

#include <sstream>
#include <string>
#include <vector>
#include <map>
#include "inspircd.h"
#include "modules.h"
#include "socketengine.h"
#include "socket.h"

/** Structure representing a single \<tag> in config */
class CoreExport ConfigTag : public refcountbase
{
	ConfigItems items;
 public:
	const std::string tag;
	const std::string src_name;
	const int src_line;

	/** Get the value of an option, using def if it does not exist */
	std::string getString(const std::string& key, const std::string& def = "");
	/** Get the value of an option, using def if it does not exist */
	long getInt(const std::string& key, long def = 0, long min = LONG_MIN, long max = LONG_MAX);
	/** Get the value of an option, using def if it does not exist */
	double getFloat(const std::string& key, double def = 0);
	/** Get the value of an option, using def if it does not exist */
	bool getBool(const std::string& key, bool def = false);

	/** Get the value in seconds of a duration that is in the user-friendly "1h2m3s" format,
	 * using a default value if it does not exist or is out of bounds.
	 * @param key The config key name
	 * @param def Default value (optional)
	 * @param min Minimum acceptable value (optional)
	 * @param max Maximum acceptable value (optional)
	 * @return The duration in seconds
	 */
	long getDuration(const std::string& key, long def = 0, long min = LONG_MIN, long max = LONG_MAX);

	/** Get the value of an option
	 * @param key The option to get
	 * @param value The location to store the value (unmodified if does not exist)
	 * @param allow_newline Allow newlines in the option (normally replaced with spaces)
	 * @return true if the option exists
	 */
	bool readString(const std::string& key, std::string& value, bool allow_newline = false);

	/** Check for an out of range value. If the value falls outside the boundaries a warning is
	 * logged and the value is corrected (set to def).
	 * @param key The key name, used in the warning message
	 * @param res The value to verify and modify if needed
	 * @param def The default value, res will be set to this if (min <= res <= max) doesn't hold true
	 * @param min Minimum accepted value for res
	 * @param max Maximum accepted value for res
	 */
	void CheckRange(const std::string& key, long& res, long def, long min, long max);

	std::string getTagLocation();

	inline const ConfigItems& getItems() const { return items; }

	/** Create a new ConfigTag, giving access to the private ConfigItems item list */
	static ConfigTag* create(const std::string& Tag, const std::string& file, int line, ConfigItems*& Items);
 private:
	ConfigTag(const std::string& Tag, const std::string& file, int line);
};

/** Defines the server's length limits on various length-limited
 * items such as topics, nicknames, channel names etc.
 */
class ServerLimits
{
 public:
	/** Maximum nickname length */
	size_t NickMax;
	/** Maximum channel length */
	size_t ChanMax;
	/** Maximum number of modes per line */
	size_t MaxModes;
	/** Maximum length of ident, not including ~ etc */
	size_t IdentMax;
	/** Maximum length of a quit message */
	size_t MaxQuit;
	/** Maximum topic length */
	size_t MaxTopic;
	/** Maximum kick message length */
	size_t MaxKick;
	/** Maximum GECOS (real name) length */
	size_t MaxGecos;
	/** Maximum away message length */
	size_t MaxAway;
	/** Maximum line length */
	size_t MaxLine;
	/** Maximum hostname length */
	size_t MaxHost;

	/** Read all limits from a config tag. Limits which aren't specified in the tag are set to a default value.
	 * @param tag Configuration tag to read the limits from
	 */
	ServerLimits(ConfigTag* tag);

	/** Maximum length of a n!u@h mask */
	size_t GetMaxMask() const { return NickMax + 1 + IdentMax + 1 + MaxHost; }
};

struct CommandLineConf
{
	/** If this value is true, the owner of the
	 * server specified -nofork on the command
	 * line, causing the daemon to stay in the
	 * foreground.
	 */
	bool nofork;

	/** If this value if true then all log
	 * messages will be output, regardless of
	 * the level given in the config file.
	 * This is set with the -debug commandline
	 * option.
	 */
	bool forcedebug;

	/** If this is true then log output will be
	 * written to the logfile. This is the default.
	 * If you put -nolog on the commandline then
	 * the logfile will not be written.
	 * This is meant to be used in conjunction with
	 * -debug for debugging without filling up the
	 * hard disk.
	 */
	bool writelog;

	/** Saved argc from startup
	 */
	int argc;

	/** Saved argv from startup
	 */
	char** argv;
};

class CoreExport OperInfo : public refcountbase
{
 public:
	typedef insp::flat_set<std::string> PrivSet;
	PrivSet AllowedOperCommands;
	PrivSet AllowedPrivs;

	/** Allowed user modes from oper classes. */
	std::bitset<64> AllowedUserModes;

	/** Allowed channel modes from oper classes. */
	std::bitset<64> AllowedChanModes;

	/** \<oper> block used for this oper-up. May be NULL. */
	reference<ConfigTag> oper_block;
	/** \<type> block used for this oper-up. Valid for local users, may be NULL on remote */
	reference<ConfigTag> type_block;
	/** \<class> blocks referenced from the \<type> block. These define individual permissions */
	std::vector<reference<ConfigTag> > class_blocks;
	/** Name of the oper type; i.e. the one shown in WHOIS */
	std::string name;

	/** Get a configuration item, searching in the oper, type, and class blocks (in that order) */
	std::string getConfig(const std::string& key);
	void init();
};

/** This class holds the bulk of the runtime configuration for the ircd.
 * It allows for reading new config values, accessing configuration files,
 * and storage of the configuration data needed to run the ircd, such as
 * the servername, connect classes, /ADMIN data, MOTDs and filenames etc.
 */
class CoreExport ServerConfig
{
  private:
	void CrossCheckOperClassType();
	void CrossCheckConnectBlocks(ServerConfig* current);

 public:
	class ServerPaths
	{
	 public:
		/** Config path */
		std::string Config;

		/** Data path */
		std::string Data;

		/** Log path */
		std::string Log;

		/** Module path */
		std::string Module;

		ServerPaths()
			: Config(INSPIRCD_CONFIG_PATH)
			, Data(INSPIRCD_DATA_PATH)
			, Log(INSPIRCD_LOG_PATH)
			, Module(INSPIRCD_MODULE_PATH) { }

		std::string PrependConfig(const std::string& fn) const { return FileSystem::ExpandPath(Config, fn); }
		std::string PrependData(const std::string& fn) const { return FileSystem::ExpandPath(Data, fn); }
		std::string PrependLog(const std::string& fn) const { return FileSystem::ExpandPath(Log, fn); }
		std::string PrependModule(const std::string& fn) const { return FileSystem::ExpandPath(Module, fn); }
	};

	/** Holds a complete list of all connect blocks
	 */
	typedef std::vector<reference<ConnectClass> > ClassVector;

	/** Index of valid oper blocks and types
	 */
	typedef insp::flat_map<std::string, reference<OperInfo> > OperIndex;

	/** Get a configuration tag
	 * @param tag The name of the tag to get
	 */
	ConfigTag* ConfValue(const std::string& tag);

	ConfigTagList ConfTags(const std::string& tag);

	/** An empty configuration tag. */
	ConfigTag* EmptyTag;

	/** Error stream, contains error output from any failed configuration parsing.
	 */
	std::stringstream errstr;

	/** True if this configuration is valid enough to run with */
	bool valid;

	/** Bind to IPv6 by default */
	bool WildcardIPv6;

	/** Used to indicate who we announce invites to on a channel */
	enum InviteAnnounceState { INVITE_ANNOUNCE_NONE, INVITE_ANNOUNCE_ALL, INVITE_ANNOUNCE_OPS, INVITE_ANNOUNCE_DYNAMIC };
	enum OperSpyWhoisState { SPYWHOIS_NONE, SPYWHOIS_SINGLEMSG, SPYWHOIS_SPLITMSG };

  	/** This holds all the information in the config file,
	 * it's indexed by tag name to a vector of key/values.
	 */
	ConfigDataHash config_data;

	/** This holds all extra files that have been read in the configuration
	 * (for example, MOTD and RULES files are stored here)
	 */
	ConfigFileCache Files;

	/** Length limits, see definition of ServerLimits class
	 */
	ServerLimits Limits;

	/** Locations of various types of file (config, module, etc). */
	ServerPaths Paths;

	/** Configuration parsed from the command line.
	 */
	CommandLineConf cmdline;

	/** Clones CIDR range for ipv4 (0-32)
	 * Defaults to 32 (checks clones on all IPs seperately)
	 */
	int c_ipv4_range;

	/** Clones CIDR range for ipv6 (0-128)
	 * Defaults to 128 (checks on all IPs seperately)
	 */
	int c_ipv6_range;

	/** Holds the server name of the local server
	 * as defined by the administrator.
	 */
	std::string ServerName;

	/** Notice to give to users when they are banned by an XLine
	 */
	std::string XLineMessage;

	/* Holds the network name the local server
	 * belongs to. This is an arbitary field defined
	 * by the administrator.
	 */
	std::string Network;

	/** Holds the description of the local server
	 * as defined by the administrator.
	 */
	std::string ServerDesc;

	/** Pretend disabled commands don't exist.
	 */
	bool DisabledDontExist;

	/** This variable contains a space-seperated list
	 * of commands which are disabled by the
	 * administrator of the server for non-opers.
	 */
	std::string DisabledCommands;

	/** This variable identifies which usermodes have been diabled.
	 */
	char DisabledUModes[64];

	/** This variable identifies which chanmodes have been disabled.
	 */
	char DisabledCModes[64];

	/** If set to true, then all opers on this server are
	 * shown with a generic 'is an IRC operator' line rather
	 * than the oper type. Oper types are still used internally.
	 */
	bool GenericOper;

	/** If this value is true, banned users (+b, not extbans) will not be able to change nick
	 * if banned on any channel, nor to message them.
	 */
	bool RestrictBannedUsers;

	/** The number of seconds the DNS subsystem
	 * will wait before timing out any request.
	 */
	int dns_timeout;

	/** The size of the read() buffer in the user
	 * handling code, used to read data into a user's
	 * recvQ.
	 */
	int NetBufferSize;

	/** The value to be used for listen() backlogs
	 * as default.
	 */
	int MaxConn;

	/** If we should check for clones during CheckClass() in AddUser()
	 * Setting this to false allows to not trigger on maxclones for users
	 * that may belong to another class after DNS-lookup is complete.
	 * It does, however, make the server spend more time on users we may potentially not want.
	 */
	bool CCOnConnect;

	/** The soft limit value assigned to the irc server.
	 * The IRC server will not allow more than this
	 * number of local users.
	 */
	unsigned int SoftLimit;

	/** Maximum number of targets for a multi target command
	 * such as PRIVMSG or KICK
	 */
	unsigned int MaxTargets;

	/** True if we're going to hide netsplits as *.net *.split for non-opers
	 */
	bool HideSplits;

	/** True if we're going to hide ban reasons for non-opers (e.g. G-Lines,
	 * K-Lines, Z-Lines)
	 */
	bool HideBans;

	/** Announce invites to the channel with a server notice
	 */
	InviteAnnounceState AnnounceInvites;

	/** If this is enabled then operators will
	 * see invisible (+i) channels in /whois.
	 */
	OperSpyWhoisState OperSpyWhois;

	/** True if raw I/O is being logged */
	bool RawLog;

	/** Set to a non-empty string to obfuscate the server name of users in WHOIS
	 */
	std::string HideWhoisServer;

	/** Set to a non empty string to obfuscate nicknames prepended to a KILL.
	 */
	std::string HideKillsServer;

	/** Set to hide kills from clients of ulined servers in snotices.
	 */
	bool HideULineKills;

	/** The full pathname and filename of the PID
	 * file as defined in the configuration.
	 */
	std::string PID;

	/** The connect classes in use by the IRC server.
	 */
	ClassVector Classes;

	/** STATS characters in this list are available
	 * only to operators.
	 */
	std::string UserStats;

	/** Default channel modes
	 */
	std::string DefaultModes;

	/** Custom version string, which if defined can replace the system info in VERSION.
	 */
	std::string CustomVersion;

	/** If set to true, provide syntax hints for unknown commands
	 */
	bool SyntaxHints;

	/** If set to true, the CycleHosts mode change will be sourced from the user,
	 * rather than the server
	 */
	bool CycleHostsFromUser;

	/** If set to true, the full nick!user\@host will be shown in the TOPIC command
	 * for who set the topic last. If false, only the nick is shown.
	 */
	bool FullHostInTopic;

	/** Oper blocks keyed by their name
	 */
	OperIndex oper_blocks;

	/** Oper types keyed by their name
	 */
	OperIndex OperTypes;

	/** Default value for <connect:maxchans>, deprecated in 3.0
	 */
	unsigned int MaxChans;

	/** Default value for <oper:maxchans>, deprecated in 3.0
	 */
	unsigned int OperMaxChans;

	/** TS6-like server ID.
	 * NOTE: 000...999 are usable for InspIRCd servers. This
	 * makes code simpler. 0AA, 1BB etc with letters are reserved
	 * for services use.
	 */
	std::string sid;

	/** Construct a new ServerConfig
	 */
	ServerConfig();

	~ServerConfig();

	/** Get server ID as string with required leading zeroes
	 */
	const std::string& GetSID() const { return sid; }

	/** Read the entire configuration into memory
	 * and initialize this class. All other methods
	 * should be used only by the core.
	 */
	void Read();

	/** Apply configuration changes from the old configuration.
	 */
	void Apply(ServerConfig* old, const std::string &useruid);
	void ApplyModules(User* user);

	void Fill();

	bool ApplyDisabledCommands(const std::string& data);

	/** Escapes a value for storage in a configuration key.
	 * @param str The string to escape.
	 * @param xml Are we using the XML config format?
	 */
	static std::string Escape(const std::string& str, bool xml = true);

	/** If this value is true, snotices will not stack when repeats are sent
	 */
	bool NoSnoticeStack;
};

/** The background thread for config reading, so that reading from executable includes
 * does not block.
 */
class CoreExport ConfigReaderThread : public Thread
{
	ServerConfig* Config;
	volatile bool done;
 public:
	const std::string TheUserUID;
	ConfigReaderThread(const std::string &useruid)
		: Config(new ServerConfig), done(false), TheUserUID(useruid)
	{
	}

	virtual ~ConfigReaderThread()
	{
		delete Config;
	}

	void Run();
	/** Run in the main thread to apply the configuration */
	void Finish();
	bool IsDone() { return done; }
};

class CoreExport ConfigStatus
{
 public:
	User* const srcuser;

	ConfigStatus(User* user = NULL)
		: srcuser(user)
	{
	}
};
