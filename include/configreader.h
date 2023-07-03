/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018 Chris Novakovic
 *   Copyright (C) 2013-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2012-2014, 2016-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <om@inspircd.org>
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

#include "inspircd.h"
#include "stringutils.h"

/** Represents the position within a file. */
class CoreExport FilePosition final
{
public:
	/** The name of the file that the position points to. */
	std::string name;

	/** The line of the file that this position points to. */
	unsigned long line;

	/** The column of the file that this position points to. */
	unsigned long column;

	/** Initialises a new file position with the specified name, line, and column.
	 * @param Name The name of the file that the position points to.
	 * @param Line The line of the file that this position points to.
	 * @param Column The column of the file that this position points to.
	 */
	FilePosition(const std::string& Name, unsigned long Line, unsigned long Column);

	/** Returns a string that represents this file position. */
	std::string str() const;
};

/** Structure representing a single \<tag> in config */
class CoreExport ConfigTag final
{
public:
	/** A mapping of configuration keys to their assigned values. */
	typedef insp::flat_map<std::string, std::string, irc::insensitive_swo> Items;

private:
	Items items;

	/** Retrieves the value of a signed integer from the server config.
	 * @param key The config key to retrieve.
	 * @param def The default value to return if not set, empty, or out of range.
	 * @param min The minimum valid value.
	 * @param max The maximum valid value.
	 */
	long double getFloat(const std::string& key, long double def, long double min, long double max) const;

	/** Retrieves the value of a signed integer from the server config.
	 * @param key The config key to retrieve.
	 * @param def The default value to return if not set, empty, or out of range.
	 * @param min The minimum valid value.
	 * @param max The maximum valid value.
	 */
	intmax_t getSInt(const std::string& key, intmax_t def, intmax_t min, intmax_t max) const;

	/** Retrieves the value of an unsigned integer from the server config.
	 * @param key The config key to retrieve.
	 * @param def The default value to return if not set, empty, or out of range.
	 * @param min The minimum valid value.
	 * @param max The maximum valid value.
	 */
	uintmax_t getUInt(const std::string& key, uintmax_t def, uintmax_t min, uintmax_t max) const;

public:
	/** The name of the configuration tag (e.g. "foo" for \<foo bar="baz">). */
	const std::string name;

	/** The position within the source file that this tag was read from. */
	const FilePosition source;

	/** Creates a new ConfigTag instance with the specified tag name, file, and line.
	 * @param Name The name of this config tag (e.g. "foo" for \<foo bar="baz">).
	 * @param Source The source of this config tag.
	 */
	ConfigTag(const std::string& Name, const FilePosition& Source);

	/** @copydoc getFloat */
	template<typename T>
	std::enable_if_t<std::is_floating_point_v<T>, T> getNum(const std::string& key, T def, T min = std::numeric_limits<T>::min(), T max = std::numeric_limits<T>::max()) const
	{
		return static_cast<T>(getFloat(key, def, min, max));
	}

	/** @copydoc getSInt */
	template<typename T>
	std::enable_if_t<std::is_signed_v<T> && !std::is_floating_point_v<T>, T> getNum(const std::string& key, T def, T min = std::numeric_limits<T>::min(), T max = std::numeric_limits<T>::max()) const
	{
		return static_cast<T>(getSInt(key, def, min, max));
	}

	/** @copydoc getUInt */
	template<typename T>
	std::enable_if_t<std::is_unsigned_v<T> && !std::is_floating_point_v<T>, T> getNum(const std::string& key, T def, T min = std::numeric_limits<T>::min(), T max = std::numeric_limits<T>::max()) const
	{
		return static_cast<T>(getUInt(key, def, min, max));
	}

	/** Get the value of an option, using def if it does not exist */
	std::string getString(const std::string& key, const std::string& def, const std::function<bool(const std::string&)>& validator) const;
	/** Get the value of an option, using def if it does not exist */
	std::string getString(const std::string& key, const std::string& def = "", size_t minlen = 0, size_t maxlen = UINT32_MAX) const;
	/** Get the value of an option, using def if it does not exist */
	bool getBool(const std::string& key, bool def = false) const;
	/** Get the value of an option, using def if it does not exist */
	unsigned char getCharacter(const std::string& key, unsigned char def = '\0') const;

	/** Get the value in seconds of a duration that is in the user-friendly "1h2m3s" format,
	 * using a default value if it does not exist or is out of bounds.
	 * @param key The config key name
	 * @param def Default value (optional)
	 * @param min Minimum acceptable value (optional)
	 * @param max Maximum acceptable value (optional)
	 * @return The duration in seconds
	 */
	unsigned long getDuration(const std::string& key, unsigned long def, unsigned long min = 0, unsigned long max = ULONG_MAX) const;

	template<typename TReturn>
	TReturn getEnum(const std::string& key, TReturn def, std::initializer_list<std::pair<const char*, TReturn>> enumvals)
	{
		const std::string val = getString(key);
		if (val.empty())
			return def;

		for (const auto& [enumkey, enumval] : enumvals)
			if (stdalgo::string::equalsci(val, enumkey))
				return enumval;

		// Unfortunately we have to iterate this twice.
		std::string enumdef = "(unknown)";
		std::string enumkeys;
		for (const auto& [enumkey, enumval] : enumvals)
		{
			enumkeys.append(enumkey).append(", ");
			if (enumval == def)
				enumdef = enumkey;
		}
		if (!enumkeys.empty())
			enumkeys.erase(enumkeys.length() - 2);

		LogMalformed(key, val, enumdef, "not one of " +  enumkeys);
		return def;
	}

	/** Get the value of an option
	 * @param key The option to get
	 * @param value The location to store the value (unmodified if does not exist)
	 * @param allow_newline Allow newlines in the option (normally replaced with spaces)
	 * @return true if the option exists
	 */
	bool readString(const std::string& key, std::string& value, bool allow_newline = false) const;

	/** Retrieves the underlying map of config entries. */
	inline const Items& GetItems() const { return items; }
	inline Items& GetItems() { return items; }

	/** @internal Logs that the value of a config field is malformed. */
	void LogMalformed(const std::string& key, const std::string& val, const std::string& def, const std::string& reason) const;
};

struct CommandLineConf final
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

	/** If this is true, a PID file will be written
	 * to the file given in the "file" variable of
	 * the \<pid> tag in the config file. This is
	 * the default.
	 * Passing --nopid as a command line argument
	 * sets this to false; in this case, a PID file
	 * will not be written, even the default PID
	 * file that is usually written when the \<pid>
	 * tag is not defined in the config file.
	 */
	bool writepid;

	/* Whether the --runasroot option was specified at boot. */
	bool runasroot;

	/** Saved argc from startup. */
	int argc;

	/** Saved argv from startup. */
	char** argv;
};

/** This class holds the bulk of the runtime configuration for the ircd.
 * It allows for reading new config values, accessing configuration files,
 * and storage of the configuration data needed to run the ircd, such as
 * the servername, connect classes, /ADMIN data, MOTDs and filenames etc.
 */
class CoreExport ServerConfig final
{
private:
	friend class ConfigReaderThread; // valid
	friend class FileReader; // Files
	friend struct ParseStack; // config_data, errstr, Files

	/** Holds the server config. */
	typedef std::multimap<std::string, std::shared_ptr<ConfigTag>, irc::insensitive_swo> TagMap;

	/** The server config. */
	TagMap config_data;

	/** Whether any errors occurred whilst reading the server config. */
	std::stringstream errstr;

	/** Files which have been read from disk. */
	ConfigFileCache Files;

	/** Whether the server config is valid. */
	bool valid;

	/** Loads added modules and unloads any removed ones.
	 * @param user If non-nullptr then the user who initiated this config load.
	 */
	void ApplyModules(User* user) const;

	/** Ensures that connect classes are well formed.
	 * @param current The current server config that is about to be replaced.
	 */
	void CrossCheckConnectBlocks(ServerConfig* current);

	/** Ensures that oper accounts, oper types, and oper classes are well formed. */
	void CrossCheckOperBlocks();

	/** Reads the core server config. */
	void Fill();

public:
	/** How to treat a user in a channel who is banned. */
	enum BannedUserTreatment
		: uint8_t
	{
		/** Don't treat a banned user any different to normal. */
		BUT_NORMAL,

		/** Restrict the actions of a banned user. */
		BUT_RESTRICT_SILENT,

		/** Restrict the actions of a banned user and notify them of their treatment. */
		BUT_RESTRICT_NOTIFY
	};

	/** Holds the limits for how long various fields can be. Read from the \<limits> tag. */
	class CoreExport ServerLimits final
	{
	public:
		/** Maximum line length */
		size_t MaxLine;

		/** Maximum nickname length */
		size_t MaxNick;

		/** Maximum channel length */
		size_t MaxChannel;

		/** Maximum number of modes per line */
		size_t MaxModes;

		/** Maximum length of a username */
		size_t MaxUser;

		/** Maximum length of a quit message */
		size_t MaxQuit;

		/** Maximum topic length */
		size_t MaxTopic;

		/** Maximum kick message length */
		size_t MaxKick;

		/** Maximum real name length */
		size_t MaxReal;

		/** Maximum away message length */
		size_t MaxAway;

		/** Maximum hostname length */
		size_t MaxHost;

		/** Read all limits from a config tag. Limits which aren't specified in the tag are set to a default value.
		 * @param tag Configuration tag to read the limits from
		 */
		ServerLimits(const std::shared_ptr<ConfigTag>& tag);

		/** Maximum length of a n!u\@h mask */
		size_t GetMaxMask() const { return MaxNick + 1 + MaxUser + 1 + MaxHost; }
	};

	/** Holds the location of various directories. Read from the \<path> tag */
	class CoreExport ServerPaths final
	{
	private:
		/** Expands a path fragment to a full path.
		 * @param base The base path to expand from
		 * @param fragment The path fragment to expand on top of base.
		 */
		static std::string ExpandPath(const std::string& base, const std::string& fragment);

	public:
		/** Config path */
		std::string Config;

		/** Data path */
		std::string Data;

		/** Log path */
		std::string Log;

		/** Module path */
		std::string Module;

		/** Runtime path */
		std::string Runtime;

		ServerPaths(const std::shared_ptr<ConfigTag>& tag);

		inline std::string PrependConfig(const std::string& fn) const { return ExpandPath(Config, fn); }
		inline std::string PrependData(const std::string& fn) const { return ExpandPath(Data, fn); }
		inline std::string PrependLog(const std::string& fn) const { return ExpandPath(Log, fn); }
		inline std::string PrependModule(const std::string& fn) const { return ExpandPath(Module, fn); }
		inline std::string PrependRuntime(const std::string& fn) const { return ExpandPath(Runtime, fn); }
	};

	/** Holds the connect classes from the server config. */
	typedef std::vector<std::shared_ptr<ConnectClass>> ClassVector;

	/** Holds the oper accounts from the server config. */
	typedef insp::flat_map<std::string, std::shared_ptr<OperAccount>> OperAccountMap;

	/** Holds the oper types from the server config. */
	typedef insp::flat_map<std::string, std::shared_ptr<OperType>> OperTypeMap;

	/** Holds iterators to a subsection of the server config map. */
	typedef insp::iterator_range<TagMap::const_iterator> TagList;

	/** The connect classes from the server config. */
	ClassVector Classes;

	/** The configuration read from the command line. */
	CommandLineConf CommandLine;

	/** An empty configuration tag. */
	std::shared_ptr<ConfigTag> EmptyTag;

	/** The limits for how long various fields can be. */
	ServerLimits Limits;

	/** The location of various directories. */
	ServerPaths Paths;

	/** Oper accounts keyed by their name. */
	OperAccountMap OperAccounts;

	/** Oper types keyed by their name. */
	OperTypeMap OperTypes;

	/** The name of the casemapping method used by this server. */
	std::string CaseMapping = "ascii";

	/** The value to show in the comment field of the RPL_VERSION. */
	std::string CustomVersion;

	/** The modes to set on a new channel. May contain channel prefix modes to set on the channel creator. */
	std::string DefaultModes;

	/** If non-empty then the quit message to use when killing an X-lined user. */
	std::string HideLines;

	/** If non-empty then the value to replace the server name with in public messages. */
	std::string HideServer;

	/* The name of the IRC network (e.g. ExampleNet). */
	std::string Network;

	/** The description of the IRC server (e.g. ExampleNet European Server). */
	std::string ServerDesc;

	/** The unique identifier for this server. Must be in the format [0-9][A-Z0-9][A-Z0-9]. */
	std::string ServerId;

	/** The hostname of the IRC server (e.g. irc.example.com). */
	std::string ServerName;

	/** The message to send to users when they are banned by an X-line. */
	std::string XLineMessage;

	/** The CIDR range to use when determining if IPv4 clients are from the same origin. */
	unsigned char IPv4Range;

	/** The CIDR range to use when determining if IPv4 clients are from the same origin. */
	unsigned char IPv6Range;

	/** How to treat a user in a channel who is banned. */
	BannedUserTreatment RestrictBannedUsers;

	/** The maximum number of connections that can be waiting in the server accept queue. */
	int MaxConn;

	/** The number of seconds that the server clock can skip by before server operators are warned. */
	time_t TimeSkipWarn;

	/** The maximum number of targets for a multi-target command (e.g. KICK). */
	unsigned long MaxTargets;

	/** The maximum amount of data to read from a socket in one go. */
	size_t NetBufferSize;

	/** The maximum number of local connections that can be made to the IRC server. */
	size_t SoftLimit;

	/** Whether to store the full nick!duser\@dhost as a topic setter instead of just their nick. */
	bool FullHostInTopic;

	/** Whether to disable stacking snotices when multiple identical messages are sent. */
	bool NoSnoticeStack;

	/** Whether raw I/O traffic is being logged. */
	bool RawLog = false;

	/** Whether to show syntax hints when a user does not provide enough parameters for a command. */
	bool SyntaxHints;

	/** Whether to bind to IPv6 by default. */
	bool WildcardIPv6;

	ServerConfig();

	/** Apply configuration changes from the old configuration. */
	void Apply(ServerConfig* old, const std::string& useruid);

	/** Get a list of configuration tags by name.
	 * @param tag The name of the tags to get.
	 * @param def The value to return if the tag doesn't exist.
	 * @returns Either a list of tags from the config or an empty TagList.
	 */
	TagList ConfTags(const std::string& tag, std::optional<TagList> def = std::nullopt) const;

	/** Get a configuration tag by name. If one or more tags are present then the first is returned.
	 * @param tag The name of the tag to get.
	 * @param def The value to return if the tag doesn't exist.
	 * @returns Either a tag from the config or EmptyTag.
	 */
	const std::shared_ptr<ConfigTag>& ConfValue(const std::string& tag, const std::shared_ptr<ConfigTag>& def = nullptr) const;

	/** Escapes a value for storage in a configuration key.
	 * @param str The string to escape.
	 */
	static std::string Escape(const std::string& str);

	/** Retrieves the entire server config. */
	const auto& GetConfig() const { return config_data; }

	/** Retrieves the list of modules that were specified in the config. */
	std::vector<std::string> GetModules() const;

	/** Retrieves the server description which should be shown to users. */
	const auto& GetServerDesc() const { return HideServer.empty() ? ServerDesc : Network; }

	/** Retrieves the server name which should be shown to users. */
	const auto& GetServerName() const { return HideServer.empty() ? ServerName : HideServer; }

	/** Attempt to read the configuration from disk. */
	void Read();
};

/** The background thread for config reading, so that reading from executable includes
 * does not block.
 */
class CoreExport ConfigReaderThread final
	: public Thread
{
private:
	/** The new server configuration. */
	ServerConfig* Config = new ServerConfig();

	/** Whether the config has been read yet. */
	std::atomic_bool done = { false };

protected:
	/** @copydoc Thread::OnStart */
	void OnStart() override;

	/** @copydoc Thread::OnStop */
	void OnStop() override;

public:
	const std::string UUID;

	ConfigReaderThread(const std::string& uuid)
		: UUID(uuid)
	{
	}

	~ConfigReaderThread() override
	{
		delete Config;
	}

	/** Whether the configuration has been read yet. */
	bool IsDone() { return done.load(); }
};

/** Represents the status of a config load. */
class CoreExport ConfigStatus final
{
public:
	/** Whether this is the initial config load. */
	bool const initial;

	/** The user who initiated the config load or NULL if not initiated by a user. */
	User* const srcuser;

	/** Initializes a new instance of the ConfigStatus class.
	 * @param user The user who initiated the config load or NULL if not initiated by a user.
	 * @param isinitial Whether this is the initial config load.
	 */
	ConfigStatus(User* user = nullptr, bool isinitial = false)
		: initial(isinitial)
		, srcuser(user)
	{
	}
};
