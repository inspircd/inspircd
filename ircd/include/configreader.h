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
#include "token_list.h"

/** Structure representing a single \<tag> in config */
class CoreExport ConfigTag : public refcountbase {
    ConfigItems items;
  public:
    const std::string tag;
    const std::string src_name;
    const int src_line;

    /** Get the value of an option, using def if it does not exist */
    std::string getString(const std::string& key, const std::string& def,
                          const TR1NS::function<bool(const std::string&)>& validator);
    /** Get the value of an option, using def if it does not exist */
    std::string getString(const std::string& key, const std::string& def = "",
                          size_t minlen = 0, size_t maxlen = UINT32_MAX);
    /** Get the value of an option, using def if it does not exist */
    long getInt(const std::string& key, long def, long min = LONG_MIN,
                long max = LONG_MAX);
    /** Get the value of an option, using def if it does not exist */
    unsigned long getUInt(const std::string& key, unsigned long def,
                          unsigned long min = 0, unsigned long max = ULONG_MAX);
    /** Get the value of an option, using def if it does not exist */
    double getFloat(const std::string& key, double def, double min = DBL_MIN,
                    double max = DBL_MAX);
    /** Get the value of an option, using def if it does not exist */
    bool getBool(const std::string& key, bool def = false);
    /** Get the value of an option, using def if it does not exist */
    unsigned char getCharacter(const std::string& key, unsigned char def = '\0');

    /** Get the value in seconds of a duration that is in the user-friendly "1h2m3s" format,
     * using a default value if it does not exist or is out of bounds.
     * @param key The config key name
     * @param def Default value (optional)
     * @param min Minimum acceptable value (optional)
     * @param max Maximum acceptable value (optional)
     * @return The duration in seconds
     */
    unsigned long getDuration(const std::string& key, unsigned long def,
                              unsigned long min = 0, unsigned long max = ULONG_MAX);

    /** Get the value of an option
     * @param key The option to get
     * @param value The location to store the value (unmodified if does not exist)
     * @param allow_newline Allow newlines in the option (normally replaced with spaces)
     * @return true if the option exists
     */
    bool readString(const std::string& key, std::string& value,
                    bool allow_newline = false);

    std::string getTagLocation();

    inline const ConfigItems& getItems() const {
        return items;
    }

    /** Create a new ConfigTag, giving access to the private ConfigItems item list */
    static ConfigTag* create(const std::string& Tag, const std::string& file,
                             int line, ConfigItems*& Items);
  private:
    ConfigTag(const std::string& Tag, const std::string& file, int line);
};

/** Defines the server's length limits on various length-limited
 * items such as topics, nicknames, channel names etc.
 */
class ServerLimits {
  public:
    /** Maximum line length */
    size_t MaxLine;
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
    /** Maximum real name length */
    size_t MaxReal;
    /** Maximum away message length */
    size_t MaxAway;
    /** Maximum hostname length */
    size_t MaxHost;

    /** Read all limits from a config tag. Limits which aren't specified in the tag are set to a default value.
     * @param tag Configuration tag to read the limits from
     */
    ServerLimits(ConfigTag* tag);

    /** Maximum length of a n!u\@h mask */
    size_t GetMaxMask() const {
        return NickMax + 1 + IdentMax + 1 + MaxHost;
    }
};

struct CommandLineConf {
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

class CoreExport OperInfo : public refcountbase {
  public:
    TokenList AllowedOperCommands;
    TokenList AllowedPrivs;

    /** Allowed user modes from oper classes. */
    std::bitset<64> AllowedUserModes;

    /** Allowed channel modes from oper classes. */
    std::bitset<64> AllowedChanModes;

    /** Allowed snomasks from oper classes. */
    std::bitset<64> AllowedSnomasks;

    /** \<oper> block used for this oper-up. May be NULL. */
    reference<ConfigTag> oper_block;
    /** \<type> block used for this oper-up. Valid for local users, may be NULL on remote */
    reference<ConfigTag> type_block;
    /** \<class> blocks referenced from the \<type> block. These define individual permissions */
    std::vector<reference<ConfigTag> > class_blocks;
    /** Name of the oper type; i.e. the one shown in WHOIS */
    std::string name;

    /** Creates a new OperInfo with the specified oper type name.
     * @param Name The name of the oper type.
     */
    OperInfo(const std::string& Name);

    /** Get a configuration item, searching in the oper, type, and class blocks (in that order) */
    std::string getConfig(const std::string& key);
    void init();
};

/** This class holds the bulk of the runtime configuration for the ircd.
 * It allows for reading new config values, accessing configuration files,
 * and storage of the configuration data needed to run the ircd, such as
 * the servername, connect classes, /ADMIN data, MOTDs and filenames etc.
 */
class CoreExport ServerConfig {
  private:
    void CrossCheckOperClassType();
    void CrossCheckConnectBlocks(ServerConfig* current);

  public:
    /** How to treat a user in a channel who is banned. */
    enum BannedUserTreatment {
        /** Don't treat a banned user any different to normal. */
        BUT_NORMAL,

        /** Restrict the actions of a banned user. */
        BUT_RESTRICT_SILENT,

        /** Restrict the actions of a banned user and notify them of their treatment. */
        BUT_RESTRICT_NOTIFY
    };

    class ServerPaths {
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

        ServerPaths(ConfigTag* tag);

        std::string PrependConfig(const std::string& fn) const {
            return FileSystem::ExpandPath(Config, fn);
        }
        std::string PrependData(const std::string& fn) const {
            return FileSystem::ExpandPath(Data, fn);
        }
        std::string PrependLog(const std::string& fn) const {
            return FileSystem::ExpandPath(Log, fn);
        }
        std::string PrependModule(const std::string& fn) const {
            return FileSystem::ExpandPath(Module, fn);
        }
        std::string PrependRuntime(const std::string& fn) const {
            return FileSystem::ExpandPath(Runtime, fn);
        }
    };

    /** Holds a complete list of all connect blocks
     */
    typedef std::vector<reference<ConnectClass> > ClassVector;

    /** Index of valid oper blocks and types
     */
    typedef insp::flat_map<std::string, reference<OperInfo> > OperIndex;

    /** Get a configuration tag by name. If one or more tags are present then the first is returned.
     * @param tag The name of the tag to get.
     * @returns Either a tag from the config or EmptyTag.
     */
    ConfigTag* ConfValue(const std::string& tag);

    /** Get a list of configuration tags by name.
     * @param tag The name of the tags to get.
     * @returns Either a list of tags from the config or an empty ConfigTagList.
     */
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
     * Defaults to 32 (checks clones on all IPs separately)
     */
    unsigned char c_ipv4_range;

    /** Clones CIDR range for ipv6 (0-128)
     * Defaults to 128 (checks on all IPs separately)
     */
    unsigned char c_ipv6_range;

    /** Holds the server name of the local server
     * as defined by the administrator.
     */
    std::string ServerName;

    /** Notice to give to users when they are banned by an XLine
     */
    std::string XLineMessage;

    /* Holds the network name the local server
     * belongs to. This is an arbitrary field defined
     * by the administrator.
     */
    std::string Network;

    /** Holds the description of the local server
     * as defined by the administrator.
     */
    std::string ServerDesc;

    /** How to treat a user in a channel who is banned. */
    BannedUserTreatment RestrictBannedUsers;

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

    /** The number of seconds that the server clock can skip by before server operators are warned. */
    time_t TimeSkipWarn;

    /** True if we're going to hide ban reasons for non-opers (e.g. G-lines,
     * K-lines, Z-lines)
     */
    bool HideBans;

    /** True if raw I/O is being logged */
    bool RawLog;

    /** Set to a non-empty string to obfuscate server names. */
    std::string HideServer;

    /** The full pathname and filename of the PID
     * file as defined in the configuration.
     */
    std::string PID;

    /** The connect classes in use by the IRC server.
     */
    ClassVector Classes;

    /** Default channel modes
     */
    std::string DefaultModes;

    /** Custom version string, which if defined can replace the system info in VERSION.
     */
    std::string CustomVersion;

    /** If set to true, provide syntax hints for unknown commands
     */
    bool SyntaxHints;

    /** The name of the casemapping method used by this server.
     */
    std::string CaseMapping;

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

    /** Unique server ID.
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
    const std::string& GetSID() const {
        return sid;
    }

    /** Retrieves the server name which should be shown to users. */
    const std::string& GetServerName() const {
        return HideServer.empty() ? ServerName : HideServer;
    }

    /** Retrieves the server description which should be shown to users. */
    const std::string& GetServerDesc() const {
        return HideServer.empty() ? ServerDesc : Network;
    }

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
class CoreExport ConfigReaderThread : public Thread {
    ServerConfig* Config;
    volatile bool done;
  public:
    const std::string TheUserUID;
    ConfigReaderThread(const std::string &useruid)
        : Config(new ServerConfig), done(false), TheUserUID(useruid) {
    }

    virtual ~ConfigReaderThread() {
        delete Config;
    }

    void Run() CXX11_OVERRIDE;
    /** Run in the main thread to apply the configuration */
    void Finish();
    bool IsDone() {
        return done;
    }
};

/** Represents the status of a config load. */
class CoreExport ConfigStatus {
  public:
    /** Whether this is the initial config load. */
    bool const initial;

    /** The user who initiated the config load or NULL if not initiated by a user. */
    User* const srcuser;

    /** Initializes a new instance of the ConfigStatus class.
     * @param user The user who initiated the config load or NULL if not initiated by a user.
     * @param isinitial Whether this is the initial config load.
     */
    ConfigStatus(User* user = NULL, bool isinitial = false)
        : initial(isinitial)
        , srcuser(user) {
    }
};
