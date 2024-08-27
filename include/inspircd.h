/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2012-2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012-2013, 2017-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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
#include "token_list.h"
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
class ServerStats final
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
	/** The last signal that was received from the operating system. */
	static sig_atomic_t lastsignal;

	/** A 64k buffer used to read socket data into. */
	char readbuffer[65535];

	/** The client protocol events provided by the core. */
	ClientProtocol::RFCEvents rfcevents;

	/** The current time, updated once per main loop iteration. */
	struct timespec ts;

	/** Prepares the server for restart or shutdown. */
	void Cleanup();

	/** Handles an stored signal in the main loop.
	 * @param signal The signal received from the operating system.
	 */
	void HandleSignal(sig_atomic_t signal);

	/** Attempt to write the process id to a file. */
	void WritePID();

public:
	/** Actions that must happen outside of the current call stack. */
	ActionList AtomicActions;

	/** Caches server bans to speeds up checking of restrictions on connect. */
	BanCacheManager BanCache;

	/** Manager for state relating to channels. */
	ChannelManager Channels;

	/** Default implementation of the protocol interface which does nothing. */
	ProtocolInterface DefaultProtocolInterface;

	/* Manager for the extension system. */
	ExtensionManager Extensions;

	/** Objects that should be culled outside of the current call stack. */
	CullList GlobalCulls;

	/** Manager for the logging system. */
	Log::Manager Logs;

	/* Manager for the mode handlers. */
	ModeParser Modes;

	/** Manager for the module loading system. */
	ModuleManager Modules;

	/** Manager for the command handlers. */
	CommandParser Parser;

	/** Manages sending snomasks to server operators. */
	SnomaskManager SNO;

	/** Holds miscellaneous stats counters. */
	ServerStats Stats;

	/** Manages scheduling and triggering of timer events. */
	TimerManager Timers;

	/* Generator for unique user identifiers. */
	UIDGenerator UIDGen;

	/** Manager for state relating to users. */
	UserManager Users;

	/** The server configuration. */
	ServerConfig* Config = nullptr;

	/* If non-nullptr then the thread that is reading the server configuration on rehash. */
	ConfigReaderThread* ConfigThread = nullptr;

	/** A fake user that represents the local server. */
	FakeUser* FakeClient = nullptr;

	/** The protocol interface used for interacting with remote servers by the linking module. */
	ProtocolInterface* PI = &DefaultProtocolInterface;

	/** Manager for X-lines. */
	XLineManager* XLines = nullptr;

	/** The current server configuration file from --config or configure. */
	std::string ConfigFileName = INSPIRCD_CONFIG_PATH "/inspircd.conf";

	/** Fills a buffer with random bytes. */
	std::function<void(char*, size_t)> GenRandom = &DefaultGenRandom;

	/** Determines whether a nickname is valid. */
	std::function<bool(const std::string_view&)> IsNick = &DefaultIsNick;

	/** Determines whether a username is valid. */
	std::function<bool(const std::string_view&)> IsUser = &DefaultIsUser;

	/** List of the open listeners. */
	std::vector<ListenSocket*> Ports;

	/** The time at which the server was started. */
	const time_t StartTime;

	/** Initialises a new server instance and stores it in ServerInstance
	 * @param argc The argument count from main().
	 * @param argv The argument list from main().
	 */
	InspIRCd(int argc, char** argv);

	/** Binds to a specific port from a config tag.
	 * @param tag the tag that contains bind information.
	 * @param sa The endpoint to listen on.
	 * @param oldports Previously listening ports that may be on the same endpoint.
	 * @param protocol The protocol to bind with or 0 to determine from the endpoint.
	 * @return True if the port was bound successfully; otherwise, false.
	 */
	bool BindPort(const std::shared_ptr<ConfigTag>& tag, const irc::sockets::sockaddrs& sa, std::vector<ListenSocket*>& oldports, sa_family_t protocol);

	/** Binds all ports specified in the configuration file.
	 * @return The number of ports bound without error.
	 */
	size_t BindPorts(FailedPortList& failed_ports);

	/** Compares a password to a hashed password.
	 * @param password The hashed password.
	 * @param passwordhash If non-empty then the algorithm the password is hashed with.
	 * @param value The value to check to see if the password is valid.
	 * @return True if the password is correct, otherwise, false.
	 */
	static bool CheckPassword(const std::string& password, const std::string& passwordhash, const std::string& value);

	/** Generates a random integer.
	 * @param max The maximum value for the integer.
	 * @return A random integer between 0 and \p max.
	 */
	unsigned long GenRandomInt(unsigned long max) const;

	/** Generates a random string.
	 * @param length The length in bytes.
	 * @param printable Whether to only return printable characters.
	 * @return  A random string of \p length bytes.
	 */
	std::string GenRandomStr(size_t length, bool printable = true) const;

	/** Retrieves a 64k buffer used to read socket data into. */
	inline auto* GetReadBuffer() { return readbuffer; }

	/** Retrieves the client protocol events provided by the core. */
	inline auto& GetRFCEvents() { return rfcevents; }

	/** Fills the output buffer with the specified number of random characters.
	 * This is the default function for InspIRCd::GenRandom.
	 * @param output The output buffer to store random characters in.
	 * @param max The maximum number of random characters to put in the buffer.
	 */
	static void DefaultGenRandom(char* output, size_t max);

	/** Determines whether a nickname is valid according to the RFC 1459 rules.
	 * This is the default function for InspIRCd::IsNick.
	 * @param nick The nickname to validate.
	 * @return True if the nickname is valid according to RFC 1459 rules; otherwise, false.
	 */
	static bool DefaultIsNick(const std::string_view& nick);

	/** Determines whether a username is valid according to the RFC 1459 rules.
	 * This is the default function for InspIRCd::IsUser.
	 * @param user The username to validate.
	 * @return True if the username is valid according to RFC 1459 rules; otherwise, false.
	*/
	static bool DefaultIsUser(const std::string_view& user);

	/** Causes the server to exit after unloading modules and closing all open file descriptors.
	 * @param status The exit code to give to the operating system.
	 */
	[[noreturn]]
	void Exit(int status);

	/** Determines whether a fully qualified hostname is valid according to RFC 5891 rules.
	 * @param host The hostname to validate.
	 * @return True if the hostname is valid; otherwise, false.
	 */
	inline static auto IsFQDN(const std::string& host) { return IsHost(host, false); }

	/** Determines whether a hostname is valid according to RFC 5891 rules.
	 * @param host The hostname to validate.
	 * @param allowsimple Whether to allow simple hostnames (e.g. localhost).
	 * @return True if the hostname is valid; otherwise, false.
	 */
	static bool IsHost(const std::string& host, bool allowsimple);

	/** Determines whether the specified string is a server identifier.
	 * @param sid The string to check.
	 * @return True if the specified string is a server identifier; otherwise, false.
	 */
	static bool IsSID(const std::string& sid);

	/** Determines whether the specified string is a valid nick!user\@host mask.
	 * @param mask The string to check.
	 * @return True if the specified string is a valid nick!user\@host mask; otherwise, false.
	 */
	static bool IsValidMask(const std::string& mask);

	/** Matches two strings using glob pattern matching, optionally with a case map to use instead
	 * of the server case map.
	 * @param str The literal string to match against
	 * @param pattern The glob pattern to match against.
	 * @param map The character map to use when matching.
	 * @return True if the string matches the mask; otherwise, false.
	 */
	static bool Match(const std::string& str, const std::string& pattern, const unsigned char* map = nullptr);

	/** Matches two strings using glob pattern matching, optionally with a case map to use instead
	 * of the server case map.
	 * @param str The literal string to match against
	 * @param pattern The glob pattern to match against.
	 * @param map The character map to use when matching.
	 * @return True if the string matches the pattern; otherwise, false.
	 */
	static bool Match(const char* str, const char* pattern, const unsigned char* map = nullptr);

	/** Matches two strings using glob pattern and CIDR range matching, optionally with a case map
	 * to use instead of the server case map.
	 * @param str The literal string to match against
	 * @param pattern The glob pattern to match against.
	 * @param map The character map to use when matching.
	 * @return True if the string matches the pattern; otherwise, false.
	 */
	static bool MatchCIDR(const std::string& str, const std::string& pattern, const unsigned char* map = nullptr);

	/** Matches two strings using glob pattern and CIDR range matching, optionally with a case map
	 * to use instead of the server case map.
	 * @param str The literal string to match against
	 * @param pattern The glob pattern to match against.
	 * @param map The character map to use when matching.
	 * @return True if the string matches the pattern; otherwise, false.
	 */
	static bool MatchCIDR(const char* str, const char* pattern, const unsigned char* map = nullptr);

	/** Matches a hostname and address against a space delimited list of hostmasks.
	 * @param masks The space delimited masks to match against.
	 * @param hostname The hostname to try and match.
	 * @param address The IP address or UNIX socket path to try and match.
	 * @return True if a mask matches the hostname or address; otherwise, false.
	 */
	static bool MatchMask(const std::string& masks, const std::string& hostname, const std::string& address);

	/** Reloads the server configuration.
	 * @param uuid If non-empty then the uuid of the user who started the rehash.
	 */
	void Rehash(const std::string& uuid = "");

	/** Starts the execution of the server main loop. */
	[[noreturn]]
	void Run();

	/** Replaces color escapes in the specified lines with IRC colors.
	 * @param lines A vector of lines to replace color escapes in.
	 */
	static void ProcessColors(std::vector<std::string>& lines);

	/** Stores an incoming signal when received from the operating system.
	 * @param signal The signal received from the operating system.
	 */
	static void SetSignal(int signal);

	/* Removes IRC colors from the specified string.
	 * @param str The string to strip colors from.
	 */
	static void StripColor(std::string& str);

	/** Retrieves the time, updated once per main loop iteration, as the number of seconds since
	 * the UNIX epoch. This is faster than calling time functions manually.
	 */
	inline auto Time() const { return ts.tv_sec; }

	/** Retrieves the time, updated once per main loop iteration, as the number of fractional
	 * seconds since the UNIX epoch. This is faster than calling time functions manually.
	 */
	inline auto Time_ns() const { return ts.tv_nsec; }

	/** Compares two strings in a timing-safe way. If the lengths of the strings differ the
	 * function returns false immediately (leaking information about the length). Otherwise, it
	 * compares each character and only returns after all characters have been compared.
	 * @param str1 The first string to compare.
	 * @param str2 The second string to compare.
	 * @return True if the strings are equivalent; otherwise, false.
	 */
	static bool TimingSafeCompare(const std::string& str1, const std::string& str2);

	/** Updates the current cached time. Don't call this unless you have reason to do so. */
	void UpdateTime();
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
