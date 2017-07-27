/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2003-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006-2007 Oliver Lupton <oliverlupton@gmail.com>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2003 randomdan <???@???>
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

#include <climits>
#include <cmath>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <stdint.h>

#include <algorithm>
#include <bitset>
#include <deque>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "intrusive_list.h"
#include "flat_map.h"
#include "compat.h"
#include "aligned_storage.h"
#include "typedefs.h"
#include "stdalgo.h"

CoreExport extern InspIRCd* ServerInstance;

/** Base class for manager classes that are still accessed using -> but are no longer pointers
 */
template <typename T>
struct fakederef
{
	T* operator->()
	{
		return static_cast<T*>(this);
	}
};

#include "config.h"
#include "convto.h"
#include "dynref.h"
#include "consolecolors.h"
#include "caller.h"
#include "cull_list.h"
#include "extensible.h"
#include "fileutils.h"
#include "numerics.h"
#include "numeric.h"
#include "uid.h"
#include "server.h"
#include "users.h"
#include "channels.h"
#include "timer.h"
#include "hashcomp.h"
#include "logger.h"
#include "usermanager.h"
#include "socket.h"
#include "ctables.h"
#include "command_parse.h"
#include "mode.h"
#include "socketengine.h"
#include "snomasks.h"
#include "filelogger.h"
#include "modules.h"
#include "threadengine.h"
#include "configreader.h"
#include "inspstring.h"
#include "protocol.h"
#include "bancache.h"
#include "isupportmanager.h"

/** This class contains various STATS counters
 * It is used by the InspIRCd class, which internally
 * has an instance of it.
 */
class serverstats
{
  public:
	/** Number of accepted connections
	 */
	unsigned long Accept;
	/** Number of failed accepts
	 */
	unsigned long Refused;
	/** Number of unknown commands seen
	 */
	unsigned long Unknown;
	/** Number of nickname collisions handled
	 */
	unsigned long Collisions;
	/** Number of DNS queries sent out
	 */
	unsigned long Dns;
	/** Number of good DNS replies received
	 * NOTE: This may not tally to the number sent out,
	 * due to timeouts and other latency issues.
	 */
	unsigned long DnsGood;
	/** Number of bad (negative) DNS replies received
	 * NOTE: This may not tally to the number sent out,
	 * due to timeouts and other latency issues.
	 */
	unsigned long DnsBad;
	/** Number of inbound connections seen
	 */
	unsigned long Connects;
	/** Total bytes of data transmitted
	 */
	unsigned long Sent;
	/** Total bytes of data received
	 */
	unsigned long Recv;
#ifdef _WIN32
	/** Cpu usage at last sample
	*/
	FILETIME LastCPU;
	/** Time QP sample was read
	*/
	LARGE_INTEGER LastSampled;
	/** QP frequency
	*/
	LARGE_INTEGER QPFrequency;
#else
	/** Cpu usage at last sample
	 */
	timeval LastCPU;
	/** Time last sample was read
	 */
	timespec LastSampled;
#endif
	/** The constructor initializes all the counts to zero
	 */
	serverstats()
		: Accept(0), Refused(0), Unknown(0), Collisions(0), Dns(0),
		DnsGood(0), DnsBad(0), Connects(0), Sent(0), Recv(0)
	{
	}
};

DEFINE_HANDLER1(IsNickHandler, bool, const std::string&);
DEFINE_HANDLER2(GenRandomHandler, void, char*, size_t);
DEFINE_HANDLER1(IsIdentHandler, bool, const std::string&);
DEFINE_HANDLER1(IsChannelHandler, bool, const std::string&);

/** The main class of the irc server.
 * This class contains instances of all the other classes in this software.
 * Amongst other things, it contains a ModeParser, a DNS object, a CommandParser
 * object, and a list of active Module objects, and facilities for Module
 * objects to interact with the core system it implements.
 */
class CoreExport InspIRCd
{
 private:
	/** Set up the signal handlers
	 */
	void SetSignals();

	/** Daemonize the ircd and close standard input/output streams
	 * @return True if the program daemonized succesfully
	 */
	bool DaemonSeed();

	/** The current time, updated in the mainloop
	 */
	struct timespec TIME;

	/** A 64k buffer used to read socket data into
	 * NOTE: update ValidateNetBufferSize if you change this
	 */
	char ReadBuffer[65535];

	/** Check we aren't running as root, and exit if we are
	 * with exit code EXIT_STATUS_ROOT.
	 */
	void CheckRoot();

 public:

	UIDGenerator UIDGen;

	/** Global cull list, will be processed on next iteration
	 */
	CullList GlobalCulls;
	/** Actions that must happen outside of the current call stack */
	ActionList AtomicActions;

	/**** Functors ****/

	IsNickHandler HandleIsNick;
	IsIdentHandler HandleIsIdent;
	IsChannelHandler HandleIsChannel;
	GenRandomHandler HandleGenRandom;

	/** Globally accessible fake user record. This is used to force mode changes etc across s2s, etc.. bit ugly, but.. better than how this was done in 1.1
	 * Reason for it:
	 * kludge alert!
	 * SendMode expects a User* to send the numeric replies
	 * back to, so we create it a fake user that isnt in the user
	 * hash and set its descriptor to FD_MAGIC_NUMBER so the data
	 * falls into the abyss :p
	 */
	FakeUser* FakeClient;

	/** Find a user in the UUID hash
	 * @param uid The UUID to find
	 * @return A pointer to the user, or NULL if the user does not exist
	 */
	User* FindUUID(const std::string &uid);

	/** Time this ircd was booted
	 */
	time_t startup_time;

	/** Config file pathname specified on the commandline or via ./configure
	 */
	std::string ConfigFileName;

	ExtensionManager Extensions;

	/** Mode handler, handles mode setting and removal
	 */
	ModeParser Modes;

	/** Command parser, handles client to server commands
	 */
	CommandParser Parser;

	/** Thread engine, Handles threading where required
	 */
	ThreadEngine Threads;

	/** The thread/class used to read config files in REHASH and on startup
	 */
	ConfigReaderThread* ConfigThread;

	/** LogManager handles logging.
	 */
	LogManager Logs;

	/** ModuleManager contains everything related to loading/unloading
	 * modules.
	 */
	ModuleManager Modules;

	/** BanCacheManager is used to speed up checking of restrictions on connection
	 * to the IRCd.
	 */
	BanCacheManager BanCache;

	/** Stats class, holds miscellaneous stats counters
	 */
	serverstats stats;

	/**  Server Config class, holds configuration file data
	 */
	ServerConfig* Config;

	/** Snomask manager - handles routing of snomask messages
	 * to opers.
	 */
	SnomaskManager SNO;

	/** Timer manager class, triggers Timer timer events
	 */
	TimerManager Timers;

	/** X-Line manager. Handles G/K/Q/E line setting, removal and matching
	 */
	XLineManager* XLines;

	/** User manager. Various methods and data associated with users.
	 */
	UserManager Users;

	/** Channel list, a hash_map containing all channels XXX move to channel manager class
	 */
	chan_hash chanlist;

	/** List of the open ports
	 */
	std::vector<ListenSocket*> ports;

	/** Set to the current signal recieved
	 */
	static sig_atomic_t s_signal;

	/** Protocol interface, overridden by server protocol modules
	 */
	ProtocolInterface* PI;

	/** Default implementation of the ProtocolInterface, does nothing
	 */
	ProtocolInterface DefaultProtocolInterface;

	/** Holds extensible for user operquit
	 */
	StringExtItem OperQuit;

	/** Manages the generation and transmission of ISUPPORT. */
	ISupportManager ISupport;

	/** Get the current time
	 * Because this only calls time() once every time around the mainloop,
	 * it is much faster than calling time() directly.
	 * @return The current time as an epoch value (time_t)
	 */
	inline time_t Time() { return TIME.tv_sec; }
	/** The fractional time at the start of this mainloop iteration (nanoseconds) */
	inline long Time_ns() { return TIME.tv_nsec; }
	/** Update the current time. Don't call this unless you have reason to do so. */
	void UpdateTime();

	/** Generate a random string with the given length
	 * @param length The length in bytes
	 * @param printable if false, the string will use characters 0-255; otherwise,
	 * it will be limited to 0x30-0x7E ('0'-'~', nonspace printable characters)
	 */
	std::string GenRandomStr(int length, bool printable = true);
	/** Generate a random integer.
	 * This is generally more secure than rand()
	 */
	unsigned long GenRandomInt(unsigned long max);

	/** Fill a buffer with random bits */
	caller2<void, char*, size_t> GenRandom;

	/** Bind all ports specified in the configuration file.
	 * @return The number of ports bound without error
	 */
	int BindPorts(FailedPortList &failed_ports);

	/** Find a user in the nick hash.
	 * If the user cant be found in the nick hash check the uuid hash
	 * @param nick The nickname to find
	 * @return A pointer to the user, or NULL if the user does not exist
	 */
	User* FindNick(const std::string &nick);

	/** Find a user in the nick hash ONLY
	 */
	User* FindNickOnly(const std::string &nick);

	/** Find a channel in the channels hash
	 * @param chan The channel to find
	 * @return A pointer to the channel, or NULL if the channel does not exist
	 */
	Channel* FindChan(const std::string &chan);

	/** Get a hash map containing all channels, keyed by their name
	 * @return A hash map mapping channel names to Channel pointers
	 */
	chan_hash& GetChans() { return chanlist; }

	/** Return true if a channel name is valid
	 * @param chname A channel name to verify
	 * @return True if the name is valid
	 */
	caller1<bool, const std::string&> IsChannel;

	/** Return true if str looks like a server ID
	 * @param sid string to check against
	 */
	static bool IsSID(const std::string& sid);

	/** Handles incoming signals after being set
	 * @param signal the signal recieved
	 */
	void SignalHandler(int signal);

	/** Sets the signal recieved
	 * @param signal the signal recieved
	 */
	static void SetSignal(int signal);

	/** Causes the server to exit after unloading modules and
	 * closing all open file descriptors.
	 *
	 * @param status The exit code to give to the operating system
	 * (See the ExitStatus enum for valid values)
	 */
	void Exit(int status);

	/** Causes the server to exit immediately with exit code 0.
	 * The status code is required for signal handlers, and ignored.
	 */
	static void QuickExit(int status);

	/** Formats the input string with the specified arguments.
	* @param formatString The string to format
	* @param ... A variable number of format arguments.
	* @return The formatted string
	*/
	static const char* Format(const char* formatString, ...) CUSTOM_PRINTF(1, 2);
	static const char* Format(va_list &vaList, const char* formatString) CUSTOM_PRINTF(2, 0);

	/** Return true if a nickname is valid
	 * @param n A nickname to verify
	 * @return True if the nick is valid
	 */
	caller1<bool, const std::string&> IsNick;

	/** Return true if an ident is valid
	 * @param An ident to verify
	 * @return True if the ident is valid
	 */
	caller1<bool, const std::string&> IsIdent;

	/** Match two strings using pattern matching, optionally, with a map
	 * to check case against (may be NULL). If map is null, match will be case insensitive.
	 * @param str The literal string to match against
	 * @param mask The glob pattern to match against.
	 * @param map The character map to use when matching.
	 */
	static bool Match(const std::string& str, const std::string& mask, unsigned const char* map = NULL);
	static bool Match(const char* str, const char* mask, unsigned const char* map = NULL);

	/** Match two strings using pattern matching, optionally, with a map
	 * to check case against (may be NULL). If map is null, match will be case insensitive.
	 * Supports CIDR patterns as well as globs.
	 * @param str The literal string to match against
	 * @param mask The glob or CIDR pattern to match against.
	 * @param map The character map to use when matching.
	 */
	static bool MatchCIDR(const std::string& str, const std::string& mask, unsigned const char* map = NULL);
	static bool MatchCIDR(const char* str, const char* mask, unsigned const char* map = NULL);

	/** Matches a hostname and IP against a space delimited list of hostmasks.
	 * @param masks The space delimited masks to match against.
	 * @param hostname The hostname to try and match.
	 * @param ipaddr The IP address to try and match.
	 */
	static bool MatchMask(const std::string& masks, const std::string& hostname, const std::string& ipaddr);

	/** Return true if the given parameter is a valid nick!user\@host mask
	 * @param mask A nick!user\@host masak to match against
	 * @return True i the mask is valid
	 */
	static bool IsValidMask(const std::string& mask);

	/** Strips all color and control codes except 001 from the given string
	 * @param sentence The string to strip from
	 */
	static void StripColor(std::string &sentence);

	/** Parses color codes from string values to actual color codes
	 * @param input The data to process
	 */
	static void ProcessColors(file_cache& input);

	/** Rehash the local server
	 * @param uuid The uuid of the user who started the rehash, can be empty
	 */
	void Rehash(const std::string& uuid = "");

	/** Calculate a duration in seconds from a string in the form 1y2w3d4h6m5s
	 * @param str A string containing a time in the form 1y2w3d4h6m5s
	 * (one year, two weeks, three days, four hours, six minutes and five seconds)
	 * @return The total number of seconds
	 */
	static unsigned long Duration(const std::string& str);

	/** Attempt to compare a password to a string from the config file.
	 * This will be passed to handling modules which will compare the data
	 * against possible hashed equivalents in the input string.
	 * @param ex The object (user, server, whatever) causing the comparison.
	 * @param data The data from the config file
	 * @param input The data input by the oper
	 * @param hashtype The hash from the config file
	 * @return True if the strings match, false if they do not
	 */
	bool PassCompare(Extensible* ex, const std::string& data, const std::string& input, const std::string& hashtype);

	/** Returns the full version string of this ircd
	 * @return The version string
	 */
	std::string GetVersionString(bool getFullVersion = false);

	/** Attempt to write the process id to a given file
	 * @param filename The PID file to attempt to write to
	 * @param exitonfail If true and the PID fail cannot be written log to stdout and exit, otherwise only log on failure
	 * @return This function may bail if the file cannot be written
	 */
	void WritePID(const std::string& filename, bool exitonfail = true);

	/** This constructor initialises all the subsystems and reads the config file.
	 * @param argc The argument count passed to main()
	 * @param argv The argument list passed to main()
	 * @throw <anything> If anything is thrown from here and makes it to
	 * you, you should probably just give up and go home. Yes, really.
	 * It's that bad. Higher level classes should catch any non-fatal exceptions.
	 */
	InspIRCd(int argc, char** argv);

	/** Prepare the ircd for restart or shutdown.
	 * This function unloads all modules which can be unloaded,
	 * closes all open sockets, and closes the logfile.
	 */
	void Cleanup();

	/** Return a time_t as a human-readable string.
	 * @param format The format to retrieve the date/time in. See `man 3 strftime`
	 * for more information. If NULL, "%a %b %d %T %Y" is assumed.
	 * @param utc True to convert the time to string as-is, false to convert it to local time first.
	 * @return A string representing the given date/time.
	 */
	static std::string TimeString(time_t curtime, const char* format = NULL, bool utc = false);

	/** Compare two strings in a timing-safe way. If the lengths of the strings differ, the function
	 * returns false immediately (leaking information about the length), otherwise it compares each
	 * character and only returns after all characters have been compared.
	 * @param one First string
	 * @param two Second string
	 * @return True if the strings match, false if they don't
	 */
	static bool TimingSafeCompare(const std::string& one, const std::string& two);

	/** Begin execution of the server.
	 * NOTE: this function NEVER returns. Internally,
	 * it will repeatedly loop.
	 */
	void Run();

	char* GetReadBuffer()
	{
		return this->ReadBuffer;
	}
};

ENTRYPOINT;

template<class Cmd>
class CommandModule : public Module
{
	Cmd cmd;
 public:
	CommandModule() : cmd(this)
	{
	}

	Version GetVersion()
	{
		return Version(cmd.name, VF_VENDOR|VF_CORE);
	}
};

inline void stdalgo::culldeleter::operator()(classbase* item)
{
	if (item)
		ServerInstance->GlobalCulls.AddItem(item);
}

#include "numericbuilder.h"
#include "modules/whois.h"
#include "modules/stats.h"
