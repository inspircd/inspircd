/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2012-2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012-2014, 2017-2019, 2022-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007-2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Oliver Lupton <om@inspircd.org>
 *   Copyright (C) 2005-2008 Craig Edwards <brain@inspircd.org>
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

#include <cfloat>
#include <climits>
#include <csignal>
#include <cstdarg>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <deque>
#include <functional>
#include <memory>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#ifndef _WIN32
# include <sys/time.h>
#endif

#include "utility/aligned_storage.h"
#include "utility/iterator_range.h"

#include "intrusive_list.h"
#include "flat_map.h"
#include "compat.h"
#include "typedefs.h"
#include "convto.h"
#include "stdalgo.h"
#include "exception.h"

CoreExport extern InspIRCd* ServerInstance;

#include "config.h"
#include "dynref.h"
#include "cull.h"
#include "extensible.h"
#include "ctables.h"
#include "numeric.h"
#include "uid.h"
#include "server.h"
#include "stringutils.h"
#include "users.h"
#include "channels.h"
#include "timer.h"
#include "hashcomp.h"
#include "channelmanager.h"
#include "usermanager.h"
#include "socket.h"
#include "command_parse.h"
#include "mode.h"
#include "socketengine.h"
#include "snomasks.h"
#include "message.h"
#include "modules.h"
#include "moduledefs.h"
#include "clientprotocol.h"
#include "thread.h"
#include "configreader.h"
#include "protocol.h"
#include "bancache.h"
#include "logging.h"

/** This class contains various STATS counters
 * It is used by the InspIRCd class, which internally
 * has an instance of it.
 */
class serverstats final
{
public:
	/** Number of accepted connections
	 */
	unsigned long Accept = 0;

	/** Number of failed accepts
	 */
	unsigned long Refused = 0;

	/** Number of unknown commands seen
	 */
	unsigned long Unknown = 0;

	/** Number of nickname collisions handled
	 */
	unsigned long Collisions = 0;

	/** Number of inbound connections seen
	 */
	unsigned long Connects = 0;

	/** Total bytes of data transmitted
	 */
	unsigned long Sent = 0;

	/** Total bytes of data received
	 */
	unsigned long Recv = 0;

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
};

/** The main class of the irc server.
 * This class contains instances of all the other classes in this software.
 * Amongst other things, it contains a ModeParser, a DNS object, a CommandParser
 * object, and a list of active Module objects, and facilities for Module
 * objects to interact with the core system it implements.
 */
class CoreExport InspIRCd final
{
private:
	/** The current time, updated in the mainloop
	 */
	struct timespec TIME;

	/** A 64k buffer used to read socket data into
	 * Update the range of <performance:netbuffersize> if you change this
	 */
	char ReadBuffer[65535];

	ClientProtocol::RFCEvents rfcevents;

public:

	UIDGenerator UIDGen;

	/** Global cull list, will be processed on next iteration
	 */
	CullList GlobalCulls;
	/** Actions that must happen outside of the current call stack */
	ActionList AtomicActions;

	/** Globally accessible fake user record. This is used to force mode changes etc across s2s, etc.. bit ugly, but.. better than how this was done in 1.1
	 * Reason for it:
	 * kludge alert!
	 * SendMode expects a User* to send the numeric replies
	 * back to, so we create it a fake user that isn't in the user
	 * hash and set its descriptor to FD_MAGIC_NUMBER so the data
	 * falls into the abyss :p
	 */
	FakeUser* FakeClient = nullptr;

	/** Time this ircd was booted
	 */
	time_t startup_time;

	/** Config file pathname specified on the commandline or via ./configure
	 */
	std::string ConfigFileName = INSPIRCD_CONFIG_PATH "/inspircd.conf";

	ExtensionManager Extensions;

	/** Mode handler, handles mode setting and removal
	 */
	ModeParser Modes;

	/** Command parser, handles client to server commands
	 */
	CommandParser Parser;

	/** The thread/class used to read config files in REHASH and on startup
	 */
	ConfigReaderThread* ConfigThread = nullptr;

	/** LogManager handles logging.
	 */
	Log::Manager Logs;

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
	ServerConfig* Config = nullptr;

	/** Snomask manager - handles routing of snomask messages
	 * to opers.
	 */
	SnomaskManager SNO;

	/** Timer manager class, triggers Timer timer events
	 */
	TimerManager Timers;

	/** X-line manager. Handles G/K/Q/E-line setting, removal and matching
	 */
	XLineManager* XLines = nullptr;

	/** User manager. Various methods and data associated with users.
	 */
	UserManager Users;

	/** Manager for state relating to channels. */
	ChannelManager Channels;

	/** List of the open ports
	 */
	std::vector<ListenSocket*> ports;

	/** Set to the current signal received
	 */
	static sig_atomic_t s_signal;

	/** Protocol interface, overridden by server protocol modules
	 */
	ProtocolInterface* PI = &DefaultProtocolInterface;

	/** Default implementation of the ProtocolInterface, does nothing
	 */
	ProtocolInterface DefaultProtocolInterface;

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
	std::string GenRandomStr(size_t length, bool printable = true) const;
	/** Generate a random integer.
	 * This is generally more secure than rand()
	 */
	unsigned long GenRandomInt(unsigned long max) const;

	/** Fill a buffer with random bits */
	std::function<void(char*, size_t)> GenRandom = &DefaultGenRandom;

	/** Fills the output buffer with the specified number of random characters.
	 * This is the default function for InspIRCd::GenRandom.
	 * @param output The output buffer to store random characters in.
	 * @param max The maximum number of random characters to put in the buffer.
	 */
	static void DefaultGenRandom(char* output, size_t max);

	/** Bind to a specific port from a config tag.
	 * @param tag the tag that contains bind information.
	 * @param sa The endpoint to listen on.
	 * @param old_ports Previously listening ports that may be on the same endpoint.
	 * @param protocol The protocol to bind with or 0 to determine from the endpoint.
	 */
	bool BindPort(const std::shared_ptr<ConfigTag>& tag, const irc::sockets::sockaddrs& sa, std::vector<ListenSocket*>& old_ports, int protocol);

	/** Bind all ports specified in the configuration file.
	 * @return The number of ports bound without error
	 */
	size_t BindPorts(FailedPortList& failed_ports);

	/** Determines whether a hostname is valid according to RFC 5891 rules.
	 * @param host The hostname to validate.
	 * @param allowsimple Whether to allow simple hostnames (e.g. localhost).
	 * @return True if the hostname is valid; otherwise, false.
	 */
	static bool IsHost(const std::string& host, bool allowsimple);

	/** Determines whether a fully qualified hostname is valid according to RFC 5891 rules.
	 * @param host The hostname to validate.
	 * @return True if the hostname is valid; otherwise, false.
	 */
	inline static bool IsFQDN(const std::string& host) { return IsHost(host, false); }

	/** Return true if str looks like a server ID
	 * @param sid string to check against
	 */
	static bool IsSID(const std::string& sid);

	/** Handles incoming signals after being set
	 * @param signal the signal received
	 */
	void SignalHandler(int signal);

	/** Sets the signal received
	 * @param signal the signal received
	 */
	static void SetSignal(int signal);

	/** Causes the server to exit after unloading modules and
	 * closing all open file descriptors.
	 *
	 * @param status The exit code to give to the operating system
	 * (See the ExitStatus enum for valid values)
	 */
	[[noreturn]]
	void Exit(int status);

	 /** Causes the server to exit immediately.
	 *
	 * @param status The exit code to give to the operating system
	 * (See the ExitStatus enum for valid values)
	 */
	[[noreturn]]
	static void QuickExit(int status);

	/** Determines whether a nickname is valid. */
	std::function<bool(const std::string_view&)> IsNick = &DefaultIsNick;

	/** Determines whether a nickname is valid according to the RFC 1459 rules.
	 * This is the default function for InspIRCd::IsNick.
	 * @param nick The nickname to validate.
	 * @return True if the nickname is valid according to RFC 1459 rules; otherwise, false.
	 */
	static bool DefaultIsNick(const std::string_view& nick);

	/** Determines whether an ident is valid. */
	std::function<bool(const std::string_view&)> IsIdent = &DefaultIsIdent;

	/** Determines whether a ident is valid according to the RFC 1459 rules.
	 * This is the default function for InspIRCd::IsIdent.
	 * @param ident The ident to validate.
	 * @return True if the ident is valid according to RFC 1459 rules; otherwise, false.
	*/
	static bool DefaultIsIdent(const std::string_view& ident);

	/** Match two strings using pattern matching, optionally, with a map
	 * to check case against (may be NULL). If map is null, match will be case insensitive.
	 * @param str The literal string to match against
	 * @param mask The glob pattern to match against.
	 * @param map The character map to use when matching.
	 */
	static bool Match(const std::string& str, const std::string& mask, const unsigned char* map = nullptr);
	static bool Match(const char* str, const char* mask, const unsigned char* map = nullptr);

	/** Match two strings using pattern matching, optionally, with a map
	 * to check case against (may be NULL). If map is null, match will be case insensitive.
	 * Supports CIDR patterns as well as globs.
	 * @param str The literal string to match against
	 * @param mask The glob or CIDR pattern to match against.
	 * @param map The character map to use when matching.
	 */
	static bool MatchCIDR(const std::string& str, const std::string& mask, const unsigned char* map = nullptr);
	static bool MatchCIDR(const char* str, const char* mask, const unsigned char* map = nullptr);

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
	static void StripColor(std::string& sentence);

	/** Parses color codes from string values to actual color codes
	 * @param input The data to process
	 */
	static void ProcessColors(std::vector<std::string>& input);

	/** Checks whether a password is valid.
	 * @param password The hashed password.
	 * @param passwordhash The name of the algorithm used to hash the password.
	 * @param value The value to check to see if the password is valid.
	 */
	static bool CheckPassword(const std::string& password, const std::string& passwordhash, const std::string& value);

	/** Rehash the local server
	 * @param uuid The uuid of the user who started the rehash, can be empty
	 */
	void Rehash(const std::string& uuid = "");

	/** Attempt to write the process id to a given file
	 */
	void WritePID();

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
	 * @param curtime The timestamp to convert to a human-readable string.
	 * @param utc True to convert the time to string as-is, false to convert it to local time first.
	 * @return A string representing the given date/time.
	 */
	static std::string TimeString(time_t curtime, const char* format = nullptr, bool utc = false);

	/** Compare two strings in a timing-safe way. If the lengths of the strings differ, the function
	 * returns false immediately (leaking information about the length), otherwise it compares each
	 * character and only returns after all characters have been compared.
	 * @param one First string
	 * @param two Second string
	 * @return True if the strings match, false if they don't
	 */
	static bool TimingSafeCompare(const std::string& one, const std::string& two);

	/** Starts the execution of the server main loop. */
	[[noreturn]]
	void Run();

	char* GetReadBuffer()
	{
		return this->ReadBuffer;
	}

	ClientProtocol::RFCEvents& GetRFCEvents() { return rfcevents; }
};

inline void Cullable::Deleter::operator()(Cullable* item)
{
	if (item)
		ServerInstance->GlobalCulls.AddItem(item);
}

inline void Channel::Write(ClientProtocol::EventProvider& protoevprov, ClientProtocol::Message& msg, char status, const CUList& except_list) const
{
	ClientProtocol::Event event(protoevprov, msg);
	Write(event, status, except_list);
}

inline void LocalUser::Send(ClientProtocol::EventProvider& protoevprov, ClientProtocol::Message& msg)
{
	ClientProtocol::Event event(protoevprov, msg);
	Send(event);
}
