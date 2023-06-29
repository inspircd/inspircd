/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2012-2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012-2013, 2016-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 DjSlash <djslash@djslash.org>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2011 jackmcbarn <jackmcbarn@inspircd.org>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2008 John Brooks <special@inspircd.org>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2003-2008 Craig Edwards <brain@inspircd.org>
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

#include "socket.h"
#include "streamsocket.h"
#include "mode.h"
#include "membership.h"

/** Represents \<connect> class tags from the server config */
class CoreExport ConnectClass final
{
public:
	/** An enumeration of possible types of connect class. */
	enum Type
		: uint8_t
	{
		/** The class defines who is allowed to connect to the server. */
		ALLOW = 0,

		/** The class defines who is banned from connecting to the server. */
		DENY = 1,

		/** The class is for server operators to be assigned to by name. */
		NAMED = 2,
	};

	/** The synthesized (with all inheritance applied) config tag this class was read from. */
	std::shared_ptr<ConfigTag> config;

	/** The hosts that this user can connect from. */
	std::vector<std::string> hosts;

	/** The name of this connect class. */
	std::string name;

	/** If non-empty then the password a user must specify in PASS to be assigned to this class. */
	std::string password;

	/** If non-empty then the hash algorithm that the password field is hashed with. */
	std::string passwordhash;

	/** If non-empty then the server ports which a user has to be connecting on. */
	insp::flat_set<in_port_t> ports;

	/** The type of class this. */
	Type type;

	/** Whether fake lag is used by this class. */
	bool fakelag:1;

	/** Whether to warn server operators about the limit for this class being reached. */
	bool maxconnwarn:1;

	/** Whether the DNS hostnames of users in this class should be resolved. */
	bool resolvehostnames:1;

	/** Whether this class is for a shared host where the username uniquely identifies users. */
	bool uniqueusername:1;

	/** Maximum rate of commands (units: millicommands per second). */
	unsigned long commandrate = 1000UL;

	/** The maximum number of bytes that users in this class can have in their send queue before they are disconnected. */
	unsigned long hardsendqmax = 1048576UL;

	/** The maximum number of users in this class that can connect to the local server from one host. */
	unsigned long limit = 5000UL;

	/** The maximum number of channels that users in this class can join. */
	unsigned long maxchans = 20UL;

	/** The maximum number of users in this class that can connect to the entire network from one host. */
	unsigned long maxglobal = 3UL;

	/** The maximum number of users that can be in this class on the local server. */
	unsigned long maxlocal = 3UL;

	/** The amount of penalty that a user in this class can have before the penalty system activates. */
	unsigned long penaltythreshold = 20UL;

	/** The number of seconds between keepalive checks for idle clients in this class. */
	unsigned long pingtime = 120UL;

	/** The maximum number of bytes that users in this class can have in their receive queue before they are disconnected. */
	unsigned long recvqmax = 4096UL;

	/** The number of seconds that connecting users have to fully connect within in this class. */
	unsigned long connection_timeout = 90UL;

	/** The maximum number of bytes that users in this class can have in their send queue before their commands stop being processed. */
	unsigned long softsendqmax = 4096UL;

	/** The number of users who are currently assigned to this class. */
	unsigned long use_count = 0UL;

	/** Creates a new connect class from a config tag. */
	ConnectClass(const std::shared_ptr<ConfigTag>& tag, Type type, const std::vector<std::string>& masks);

	/** Creates a new connect class with a parent from a config tag. */
	ConnectClass(const std::shared_ptr<ConfigTag>& tag, Type type, const std::vector<std::string>& masks, const std::shared_ptr<ConnectClass>& parent);

	/** Configures this connect class using the config from the specified tag. */
	void Configure(const std::string& classname, const std::shared_ptr<ConfigTag>& tag);

	/** Update the settings in this block to match the given class */
	void Update(const std::shared_ptr<ConnectClass>& klass);

	/** Retrieves the name of this connect class. */
	const std::string& GetName() const { return name; }

	/** Retrieves the hosts for this connect class. */
	const std::vector<std::string>& GetHosts() const { return hosts; }
};

/** Represents an \<opertype> from the server config. */
class CoreExport OperType
	: public insp::uncopiable
{
protected:
	friend class OperAccount;

	/** Oper-only channel modes that an oper of this type can use. */
	ModeParser::ModeStatus chanmodes;

	/** Oper-only commands that an oper of this type can use. */
	TokenList commands;

	/** The config tag this oper type was created from. */
	std::shared_ptr<ConfigTag> config;

	/** The name of this oper type. */
	std::string name;

	/** Oper privileges that an oper of this type has. */
	TokenList privileges;

	/** Oper snomasks that an oper of this type can use. */
	ModeParser::ModeStatus snomasks;

	/** Oper-only user modes that an oper of this type can use. */
	ModeParser::ModeStatus usermodes;

	/** Merges the specified config tag into this oper type's config.
	 * @param tag The config tag to merge in.
	 */
	void MergeTag(const std::shared_ptr<ConfigTag>& tag);

public:
	/** Creates a new oper type with the specified name and config tag.
	 * @param n The name of the oper type.
	 * @param t The tag to configure the oper type from.
	 */
	OperType(const std::string& n, const std::shared_ptr<ConfigTag>& t);

	/** Configures this oper type with settings from the specified tag.
	 * @param tag The tag to configure from.
	 * @param merge Whether to merge this tag into the synthetic tag.
	 */
	void Configure(const std::shared_ptr<ConfigTag>& tag, bool merge);

	/** Retrieves the config tag this oper type was created from. */
	const auto& GetConfig() const { return config; }

	/** Retrieves the name of this oper type. */
	const auto& GetName() const { return name; }

	/** Retrieves the commands that this oper type has access to.
	 * @param all Whether to return all commands even if they don't currently exist.
	 */
	std::string GetCommands(bool all = false) const;

	/** Retrieves the modes that this oper type has access to.
	 * @param mt The type of mode to retrieve.
	 * @param all Whether to return all commands even if they don't currently exist.
	 */
	std::string GetModes(ModeType mt, bool all = false) const;

	/** Retrieves the privileges that this oper type has access to. */
	std::string GetPrivileges() const { return privileges.ToString(); }

	/** Retrieves the snomasks that this oper type has access to.
	 * @param all Whether to return all snomasks even if they don't currently exist.
	 */
	std::string GetSnomasks(bool all = false) const;

	/** Determines if this oper type can use the specified command.
	 * @param cmd The command to check for.
	 */
	bool CanUseCommand(const std::string& cmd) const;

	/** Determines if this oper type can use the specified command.
	 * @param cmd The command to check for.
	 */
	inline bool CanUseCommand(const Command* cmd) const { return CanUseCommand(cmd->name); }

	/** Determines if this oper type can use the specified mode.
	 * @param mt The type of mode to check for.
	 * @param chr The mode character to check for.
	 */
	bool CanUseMode(ModeType mt, unsigned char chr) const;

	/** Determines if this oper type can use the specified mode.
	 * @param mh The mode to check for.
	 */
	inline bool CanUseMode(const ModeHandler* mh) const { return CanUseMode(mh->GetModeType(), mh->GetModeChar()); }

	/** Determines if this oper type can use the specified snomask.
	 * @param chr The snomask to check for.
	 */
	bool CanUseSnomask(unsigned char chr) const;

	/** Determines if this oper type has the specified privilege.
	 * @param priv The privilege to check for.
	 */
	bool HasPrivilege(const std::string& priv) const;
};

#ifdef _WIN32
# undef STRICT // Defined by Windows SDK.
#endif

/** Represents an \<oper> from the server config. */
class CoreExport OperAccount
	: public OperType
{
protected:
	/** Possible states for whether an oper account can be automatically logged into. */
	enum class AutoLogin
		: uint8_t
	{
		/** Users can automatically log in to this account if they match all fields and their nick matches the account name. */
		STRICT,

		/** Users can automatically log in to this account if they match all fields. */
		RELAXED,

		/** Users can not automatically log in to this account. */
		NEVER,
	};

	/** Whether this oper account can be automatically logged into. */
	AutoLogin autologin;

	/** The password to used to log into this oper account. */
	std::string password;

	/** The algorithm to used to hash the password of this oper account. */
	std::string passwordhash;

	/** The name of the underlying oper type. */
	std::string type;

public:
	/** Creates a new oper account with the specified name, oper type, and config tag.
	 * @param n The name of the oper account.
	 * @param o The oper type that this account inherits settings from.
	 * @param t The tag to configure the oper account from.
	 */
	OperAccount(const std::string& n, const std::shared_ptr<OperType>& o, const std::shared_ptr<ConfigTag>& t);

	/** Check whether this user can attempt to automatically log in to this account. */
	bool CanAutoLogin(LocalUser* user) const;

	/** Check the specified password against the one from this oper account's password.
	 * @param pw The password to check.
	 */
	bool CheckPassword(const std::string& pw) const;

	/** Retrieves the name of the underlying oper type. */
	const auto& GetType() const { return type; }
};

/** Holds all information about a user
 * This class stores all information about a user connected to the irc server. Everything about a
 * connection is stored here primarily, from the user's socket ID (file descriptor) through to the
 * user's nickname and hostname.
 */
class CoreExport User
	: public Extensible
{
private:
	/** Cached value for GetAddress. */
	std::string cached_address;

	/** Cached value for GetUserAddress. */
	std::string cached_useraddress;

	/** Cached value for GetUserHost. */
	std::string cached_userhost;

	/** Cached value for GetRealUserHost. */
	std::string cached_realuserhost;

	/** Cached value for GetMask. */
	std::string cached_mask;

	/** Cached value for GetRealMask. */
	std::string cached_realmask;

	/** If set then the hostname which is displayed to users. */
	std::string displayhost;

	/** The real hostname of this user. */
	std::string realhost;

	/** The real name of this user. */
	std::string realname;

	/** If set then the username which is displayed to users. */
	std::string displayuser;

	/** The real username of this user from USER or an ident loookup. */
	std::string realuser;

	/** The user's mode list.
	 * Much love to the STL for giving us an easy to use bitset, saving us RAM.
	 * if (modes[modeid]) is set, then the mode is set.
	 * For example, to work out if mode +i is set, we check the field
	 * User::modes[invisiblemode->modeid] == true.
	 */
	ModeParser::ModeStatus modes;

public:
	/** To execute a function for each local neighbor of a user, inherit from this class and
	 * pass an instance of it to User::ForEachNeighbor().
	 */
	class ForEachNeighborHandler
	{
	public:
		/** Method to execute for each local neighbor of a user.
		 * Derived classes must implement this.
		 * @param user Current neighbor
		 */
		virtual void Execute(LocalUser* user) = 0;
	};

	/** Represents the state of a connection to the server. */
	enum ConnectionState
		: uint8_t
	{
		/** The user has not sent any commands. */
		CONN_NONE = 0,

		/** The user has sent the NICK command. */
		CONN_NICK = 1,

		/** The user has sent the USER command. */
		CONN_USER = 2,

		/** The user has sent both the NICK and USER commands and is waiting to be fully connected. */
		CONN_NICKUSER = CONN_NICK | CONN_USER,

		/** The user has sent both the NICK and USER commands and is fully connected */
		CONN_FULL = 7,
	};

	/** An enumeration of all possible types of user. */
	enum Type
		: uint8_t
	{
		/** The user is connected to the local server. */
		TYPE_LOCAL = 0,

		/** The user is connected to a remote server. */
		TYPE_REMOTE = 1,

		/** The user is a server pseudo-user. */
		TYPE_SERVER = 2,
	};

	/** List of Memberships for this user
	 */
	typedef insp::intrusive_list<Membership> ChanList;

	/** A list of memberships to consider when discovering the neighbors of a user. */
	typedef std::vector<Membership*> NeighborList;

	/** A list of exceptions to the memberships that are considered when discovering the neighbours of a user. */
	typedef std::unordered_map<User*, bool> NeighborExceptions;

	/** The time at which this user's nickname was last changed. */
	time_t nickchanged;

	/** Time the connection was created, set in the constructor. This
	 * may be different from the time the user's Cullable object was
	 * created.
	 */
	time_t signon = 0;

	/** Client address that the user is connected from.
	 * Do not modify this value directly, use ChangeRemoteAddress() to change it.
	 * Port is not valid for remote users.
	 */
	irc::sockets::sockaddrs client_sa;

	/** The users nickname.
	 * Use InspIRCd::IsNick() to validate nicknames.
	 */
	std::string nick;

	/** The user's unique identifier.
	 * This is the unique identifier which the user has across the network.
	 */
	const std::string uuid;

	/** What snomasks are set on this user.
	 * This functions the same as the above modes.
	 */
	std::bitset<64> snomasks;

	/** Channels this user is on
	 */
	ChanList chans;

	/** The server the user is connected to.
	 */
	Server* server;

	/** The user's away message.
	 * If this string is empty, the user is not marked as away.
	 */
	std::string awaymsg;

	/** Time the user last went away.
	 * This is ONLY RELIABLE if user IsAway()!
	 */
	time_t awaytime;

	/** If non-null then the oper account this user is logged in to. */
	std::shared_ptr<OperAccount> oper;

	/** The connection state of the user. */
	unsigned int connected:3;

	/** If this is set to true, then all socket operations for the user
	 * are dropped into the bit-bucket.
	 * This value is set by QuitUser, and is not needed separately from that call.
	 * Please note that setting this value alone will NOT cause the user to quit.
	 */
	unsigned int quitting:1;

	/** Whether the username field uniquely identifies this user on their origin host. */
	bool uniqueusername:1;

	/** What type of user is this? */
	const uint8_t usertype:2;

	/** Retrieves the username which should be included in bans for this user. */
	const std::string& GetBanUser(bool real) const;

	/** Retrieves this user's hostname.
	 * @param uncloak If true then return the real hostname; otherwise, the display hostname.
	 */
	inline const std::string& GetHost(bool uncloak) const
	{
		return uncloak ? GetRealHost() : GetDisplayedHost();
	}

	/** Retrieves this user's username.
	 * @param uncloak If true then return the real username; otherwise, the display username.
	 */
	inline const std::string& GetUser(bool uncloak) const
	{
		return uncloak ? GetRealUser() : GetDisplayedUser();
	}

	/** Retrieves this user's displayed hostname. */
	inline const std::string& GetDisplayedHost() const
	{
		return displayhost.empty() ? realhost : displayhost;
	}

	/** Retrieves this user's displayed username. */
	inline const std::string& GetDisplayedUser() const
	{
		return displayuser.empty() ? realuser : displayuser;
	}

	/** Retrieves this user's real hostname. */
	inline const std::string& GetRealHost() const { return realhost; }

	/** Retrieves this user's real username. */
	inline const std::string& GetRealUser() const { return realuser; }

	/** Retrieves this user's real name. */
	inline const std::string& GetRealName() const { return realname; }

	/** Get CIDR mask, using default range, for this user
	 */
	irc::sockets::cidr_mask GetCIDRMask() const;

	/** Retrieves the remote address (IPv4, IPv6, UNIX socket path) as a string.
	 * If this method has not been called before then it will be cached.
	 */
	virtual const std::string& GetAddress();

	/*** Retrieves the user@address mask for the user as a string.
	 * If this method has not been called before then it will be cached.
	 */
	virtual const std::string& GetUserAddress();

	/*** Retrieves the user@dhost mask for the user as a string.
	 * If this method has not been called before then it will be cached.
	 */
	virtual const std::string& GetUserHost();

	/*** Retrieves the user@rhost mask for the user as a string.
	 * If this method has not been called before then it will be cached.
	 */
	virtual const std::string& GetRealUserHost();

	/*** Retrieves the nick!user@dhost mask for the user as a string.
	 * If this method has not been called before then it will be cached.
	 */
	virtual const std::string& GetMask();

	/*** Retrieves the nick!user@dhost mask for the user as a string.
	 * If this method has not been called before then it will be cached.
	 */
	virtual const std::string& GetRealMask();

	/** Changes the remote socket address for this user.
	 * @param sa The new socket address.
	 */
	virtual void ChangeRemoteAddress(const irc::sockets::sockaddrs& sa);

	/** Constructor
	 * @throw CoreException if the UID allocated to the user already exists
	 */
	User(const std::string& uid, Server* srv, Type objtype);

	/** This clears any cached results that are used for GetFullRealHost() etc.
	 * The results of these calls are cached as generating them can be generally expensive.
	 */
	void InvalidateCache();

	/** Returns whether this user is currently away or not. If true,
	 * further information can be found in User::awaymsg and User::awaytime
	 * @return True if the user is away, false otherwise
	 */
	bool IsAway() const { return (!awaymsg.empty()); }

	/** Returns whether this user is an oper or not. If true,
	 * oper information can be obtained from User::oper
	 * @return True if the user is an oper, false otherwise
	 */
	bool IsOper() const { return !!oper; }

	/** Returns true if a notice mask is set
	 * @param sm A notice mask character to check
	 * @return True if the notice mask is set
	 */
	bool IsNoticeMaskSet(unsigned char sm) const;

	/** Get the mode letters of modes set on the user as a string.
	 * @param includeparams True to get the parameters of the modes as well. Defaults to false.
	 * @return Mode letters of modes set on the user and optionally the parameters of those modes, if any.
	 * The returned string always begins with a '+' character. If the user has no modes set, "+" is returned.
	 */
	std::string GetModeLetters(bool includeparams = false) const;

	/** Returns true if a specific mode is set
	 * @param m The user mode
	 * @return True if the mode is set
	 */
	bool IsModeSet(unsigned char m) const;
	bool IsModeSet(const ModeHandler* mh) const;
	bool IsModeSet(const ModeHandler& mh) const { return IsModeSet(&mh); }
	bool IsModeSet(const UserModeReference& moderef) const;

	/** Set a specific usermode to on or off
	 * @param mh The user mode
	 * @param value On or off setting of the mode
	 */
	void SetMode(const ModeHandler* mh, bool value);
	void SetMode(const ModeHandler& mh, bool value) { SetMode(&mh, value); }

	/** Returns true or false for if a user can execute a privileged oper command.
	 * This is done by looking up their oper type from User::oper, then referencing
	 * this to their oper classes and checking the commands they can execute.
	 * @param command A command (should be all CAPS)
	 * @return True if this user can execute the command
	 */
	inline bool HasCommandPermission(const std::string& command) const { return IsOper() && oper->CanUseCommand(command); }

	/** Returns true if a user has a given permission.
	 * This is used to check whether or not users may perform certain actions which admins may not wish to give to
	 * all operators, yet are not commands. An example might be oper override, mass messaging (/notice $*), etc.
	 *
	 * @param privstr The priv to check, e.g. "users/override/topic". These are loaded free-form from the config file.
	 * @return True if this user has the permission in question.
	 */
	inline bool HasPrivPermission(const std::string& privstr) const { return IsOper() && oper->HasPrivilege(privstr); }

	/** Returns true or false if a user can set a privileged user or channel mode.
	 * This is done by looking up their oper type from User::oper, then referencing
	 * this to their oper classes, and checking the modes they can set.
	 * @param mh Mode to check
	 * @return True if the user can set or unset this mode.
	 */
	inline bool HasModePermission(const ModeHandler* mh) const { return IsOper() && oper->CanUseMode(mh); }

	/** Determines whether this user can set the specified snomask.
	 * @param chr The server notice mask character to look up.
	 * @return True if the user can set the specified snomask; otherwise, false.
	 */
	inline bool HasSnomaskPermission(char chr) const { return IsOper() && oper->CanUseSnomask(chr); }

	/** Logs this user into the specified server operator account.
	 * @param account The account to log this user in to.
	 * @param automatic Whether this is an automatic login attempt.
	 * @param force Whether to ignore any checks from OnPreOperLogin.
	 * @return True if the user is logged into successfully; otherwise, false.
	 */
	bool OperLogin(const std::shared_ptr<OperAccount>& account, bool automatic = false, bool force = false);

	/** Logs this user out of their server operator account. Does nothing to non-operators. */
	void OperLogout();

	/** Sends a server notice to this user.
	 * @param text The contents of the message to send.
	 */
	void WriteNotice(const std::string& text);

	/** Send a NOTICE message from the local server to the user.
	 * @param text Text to send
	 */
	virtual void WriteRemoteNotice(const std::string& text);

	virtual void WriteRemoteNumeric(const Numeric::Numeric& numeric);

	template <typename... Param>
	void WriteRemoteNumeric(unsigned int numeric, Param&&... p)
	{
		Numeric::Numeric n(numeric);
		n.push(std::forward<Param>(p)...);
		WriteRemoteNumeric(n);
	}

	void WriteNumeric(const Numeric::Numeric& numeric);

	template <typename... Param>
	void WriteNumeric(unsigned int numeric, Param&&... p)
	{
		Numeric::Numeric n(numeric);
		n.push(std::forward<Param>(p)...);
		WriteNumeric(n);
	}

	/** Write to all users that can see this user (including this user in the list if include_self is true), appending CR/LF
	 * @param protoev Protocol event to send, may contain any number of messages.
	 * @param include_self Should the message be sent back to the author?
	 */
	void WriteCommonRaw(ClientProtocol::Event& protoev, bool include_self = true);

	/** Execute a function once for each local neighbor of this user. By default, the neighbors of a user are the users
	 * who have at least one common channel with the user. Modules are allowed to alter the set of neighbors freely.
	 * This function is used for example to send something conditionally to neighbors, or to send different messages
	 * to different users depending on their oper status.
	 * @param handler Function object to call, inherited from ForEachNeighborHandler.
	 * @param include_self True to include this user in the set of neighbors, false otherwise.
	 * Modules may override this. Has no effect if this user is not local.
	 */
	uint64_t ForEachNeighbor(ForEachNeighborHandler& handler, bool include_self = true);

	/** Return true if the user shares at least one channel with another user
	 * @param other The other user to compare the channel list against
	 * @return True if the given user shares at least one channel with this user
	 */
	bool SharesChannelWith(User* other) const;

	/** Change the displayed hostname of this user.
	 * @param host The new displayed hostname of this user.
	 * @return True if the hostname was changed successfully; otherwise, false.
	 */
	bool ChangeDisplayedHost(const std::string& host);

	/** Change the real hostname of this user.
	 * @param host The new real hostname of this user.
	 * @param resetdisplay Whether to reset the display host to this value.
	 */
	void ChangeRealHost(const std::string& host, bool resetdisplay);

	/** Change the displayed username of this user.
	 * @param newuser The new displayed username of this user.
	 * @return True if the username was changed successfully; otherwise, false.
	 */
	bool ChangeDisplayedUser(const std::string& newuser);

	/** Change the real username of this user.
	 * @param newuser The new real username of this user.
	 * @param resetdisplay Whether to reset the display username to this value.
	 */
	void ChangeRealUser(const std::string& newuser, bool resetdisplay);

	/** Change a users realname field.
	 * @param real The user's new real name
	 * @return True if the change succeeded, false if otherwise
	 */
	bool ChangeRealName(const std::string& real);

	/** Change a user's nick
	 * @param newnick The new nick. If equal to the users uuid, the nick change always succeeds.
	 * @param newts The time at which this nick change happened.
	 * @return True if the change succeeded
	 */
	bool ChangeNick(const std::string& newnick, time_t newts = 0);

	/** Remove this user from all channels they are on, and delete any that are now empty.
	 * This is used by QUIT, and will not send part messages!
	 */
	void PurgeEmptyChannels();

	/** @copydoc Cullable::Cull */
	Cullable::Result Cull() override;

	/** Determines whether this user is fully connected to the server .*/
	inline bool IsFullyConnected() const { return connected == CONN_FULL; }
};

class CoreExport UserIOHandler final
	: public StreamSocket
{
private:
	size_t checked_until = 0;
public:
	LocalUser* const user;
	UserIOHandler(LocalUser* me)
		: StreamSocket(StreamSocket::SS_USER)
		, user(me)
	{
	}
	void OnDataReady() override;
	bool OnChangeLocalSocketAddress(const irc::sockets::sockaddrs& sa) override;
	bool OnChangeRemoteSocketAddress(const irc::sockets::sockaddrs& sa) override;
	void OnError(BufferedSocketError error) override;

	/** Adds to the user's write buffer.
	 * You may add any amount of text up to this users sendq value, if you exceed the
	 * sendq value, the user will be removed, and further buffer adds will be dropped.
	 * @param data The data to add to the write buffer
	 */
	void AddWriteBuf(const std::string& data);
};

class CoreExport LocalUser final
	: public User
	, public insp::intrusive_list_node<LocalUser>
{
private:
	/** The connect class this user is in. */
	std::shared_ptr<ConnectClass> connectclass;

	/** Message list, can be passed to the two parameter Send(). */
	static ClientProtocol::MessageList sendmsglist;

	/** Add a serialized message to the send queue of the user.
	 * @param serialized Bytes to add.
	 */
	void Write(const ClientProtocol::SerializedMessage& serialized);

	/** Send a protocol event to the user, consisting of one or more messages.
	 * @param protoev Event to send, may contain any number of messages.
	 * @param msglist Message list used temporarily internally to pass to hooks and store messages
	 * before Write().
	 */
	void Send(ClientProtocol::Event& protoev, ClientProtocol::MessageList& msglist);

public:
	LocalUser(int fd, const irc::sockets::sockaddrs& client, const irc::sockets::sockaddrs& server);

	Cullable::Result Cull() override;

	UserIOHandler eh;

	/** Serializer to use when communicating with the user
	 */
	ClientProtocol::Serializer* serializer = nullptr;

	/** Stats counter for bytes inbound
	 */
	unsigned int bytes_in = 0;

	/** Stats counter for bytes outbound
	 */
	unsigned int bytes_out = 0;

	/** Stats counter for commands inbound
	 */
	unsigned int cmds_in = 0;

	/** Stats counter for commands outbound
	 */
	unsigned int cmds_out = 0;

	/** Password specified by the user when they connected (if any).
	 * This is stored even if the \<connect> block doesnt need a password, so that
	 * modules may check it.
	 */
	std::string password;

	/** Get the connect class which this user belongs to.
	 * @return A pointer to this user's connect class.
	 */
	const std::shared_ptr<ConnectClass>& GetClass() const { return connectclass; }

	/** Server address and port that this user is connected to.
	 */
	irc::sockets::sockaddrs server_sa;

	/** Recursion fix: user is out of SendQ and will be quit as soon as possible.
	 * This can't be handled normally because QuitUser itself calls Write on other
	 * users, which could trigger their SendQ to overrun.
	 */
	unsigned int quitting_sendq:1;

	/** has the user responded to their previous ping?
	 */
	unsigned int lastping:1;

	/** This is true if the user matched an exception (E-line). It is used to save time on ban checks.
	 */
	unsigned int exempt:1;

	/** The time at which this user should be pinged next. */
	time_t nextping = 0;

	/** Time that the connection last sent a message, used to calculate idle time
	 */
	time_t idle_lastmsg = 0;

	/** This value contains how far into the penalty threshold the user is.
	 * This is used either to enable fake lag or for excess flood quits
	 */
	unsigned int CommandFloodPenalty = 0;

	uint64_t already_sent = 0;

	/** Check if the user matches a G- or K-line, and disconnect them if they do.
	 * @param doZline True if Z-lines should be checked (if IP has changed since initial connect)
	 * Returns true if the user matched a ban, false else.
	 */
	bool CheckLines(bool doZline = false);

	/** Use this method to fully connect a user.
	 * This will send the message of the day, check G/K/E-lines, etc.
	 */
	void FullConnect();

	/** @copydoc User::ChangeRemoteAddress */
	void ChangeRemoteAddress(const irc::sockets::sockaddrs& sa) override;

	/** Change the connect class for this user.
	 * @param klass The connect class the user should be assigned to.
	 * @param force Whether the connect class was explicitly picked (e.g. via <oper:class>).
	 */
	void ChangeConnectClass(const std::shared_ptr<ConnectClass>& klass, bool force);

	/** Find a new connect class for this user.
	 * @return True if an allow-type connect class was found for the user. Otherwise, false.
	 */
	bool FindConnectClass();

	/** Send a NOTICE message from the local server to the user.
	 * The message will be sent even if the user is connected to a remote server.
	 * @param text Text to send
	 */
	void WriteRemoteNotice(const std::string& text) override;

	/** Change nick to uuid, unset CONN_NICK and send a nickname overruled numeric.
	 * This is called when another user (either local or remote) needs the nick of this user and this user
	 * isn't fully connected.
	 */
	void OverruleNick();

	/** Send a protocol event to the user, consisting of one or more messages.
	 * @param protoev Event to send, may contain any number of messages.
	 */
	void Send(ClientProtocol::Event& protoev);

	/** Send a single message to the user.
	 * @param protoevprov Protocol event provider.
	 * @param msg Message to send.
	 */
	void Send(ClientProtocol::EventProvider& protoevprov, ClientProtocol::Message& msg);
};

class RemoteUser
	: public User
{
public:
	RemoteUser(const std::string& uid, Server* srv)
		: User(uid, srv, TYPE_REMOTE)
	{
	}
};

class CoreExport FakeUser final
	: public User
{
public:
	/** Creates a new fake user with the specified sid and server details.
	 * @param sid A server id in the format [0-9][A-Z0-9][A-Z0-9].
	 * @param srv The server instance to configure this fake user from.
	 */
	FakeUser(const std::string& sid, Server* srv);

	/** Creates a new fake user with the specified sid, server name, and server description.
	 * @param sid A server id in the format [0-9][A-Z0-9][A-Z0-9].
	 * @param sname The name of the server.
	 * @param sdesc The description of the server.
	 */
	FakeUser(const std::string& sid, const std::string& sname, const std::string& sdesc);

	/** @copydoc Cullable::Cull. */
	Cullable::Result Cull() override;

	/** @copydoc User::GetMask. */
	const std::string& GetMask() override;

	/** @copydoc User::GetRealMask. */
	const std::string& GetRealMask() override;
};

/* Faster than dynamic_cast */
/** Is a local user */
inline LocalUser* IS_LOCAL(User* u)
{
	return (u != nullptr && u->usertype == User::TYPE_LOCAL) ? static_cast<LocalUser*>(u) : nullptr;
}
/** Is a remote user */
inline RemoteUser* IS_REMOTE(User* u)
{
	return (u != nullptr && u->usertype == User::TYPE_REMOTE) ? static_cast<RemoteUser*>(u) : nullptr;
}
/** Is a server fakeuser */
inline FakeUser* IS_SERVER(User* u)
{
	return (u != nullptr && u->usertype == User::TYPE_SERVER) ? static_cast<FakeUser*>(u) : nullptr;
}

inline bool User::IsModeSet(const ModeHandler* mh) const
{
	return ((mh->GetId() != ModeParser::MODEID_MAX) && (modes[mh->GetId()]));
}

inline bool User::IsModeSet(const UserModeReference& moderef) const
{
	if (!moderef)
		return false;
	return IsModeSet(*moderef);
}

inline void User::SetMode(const ModeHandler* mh, bool value)
{
	if (mh && mh->GetId() != ModeParser::MODEID_MAX)
		modes[mh->GetId()] = value;
}
