/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 iwalkalone <iwalkalone69@gmail.com>
 *   Copyright (C) 2012-2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012-2013, 2017-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@gmail.com>
 *   Copyright (C) 2007-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Oliver Lupton <om@inspircd.org>
 *   Copyright (C) 2006-2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 John Brooks <john@jbrooks.io>
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

class DLLManager;

/** Used to specify the behaviour of a module. */
enum ModuleFlags
{
	/** The module has no special attributes. */
	VF_NONE = 0,

	/** The module is a coremod and can be assumed to be loaded on all servers. */
	VF_CORE = 1,

	/* The module is included with InspIRCd. */
	VF_VENDOR = 2,

	/** The module MUST be loaded on all servers on a network to link. */
	VF_COMMON = 4,

	/** The module SHOULD be loaded on all servers on a network for consistency. */
	VF_OPTCOMMON = 8
};

/** The event was explicitly allowed. */
#define MOD_RES_ALLOW (ModResult(1))

/** The event was not explicitly allowed or denied. */
#define MOD_RES_PASSTHRU (ModResult(0))

/** The event was explicitly denied. */
#define MOD_RES_DENY (ModResult(-1))

/** Represents the result of a module event. */
class ModResult final
{
private:
	/** The underlying result value. */
	char result = 0;

public:
	/** Creates a new instance of the ModResult class which defaults to MOD_RES_PASSTHRU. */
	ModResult() = default;

	/** Creates a new instance of the ModResult class with the specified value. */
	explicit ModResult(char res)
		: result(res)
	{
	}

	/** Determines whether this ModResult has.the same value as \p res */
	inline bool operator==(const ModResult& res) const
	{
		return result == res.result;
	}

	/** Determines whether this ModResult has.a different value to \p res */
	inline bool operator!=(const ModResult& res) const
	{
		return result != res.result;
	}

	/** Determines whether a non-MOD_RES_PASSTHRU result has been set. */
	inline bool operator!() const
	{
		return !result;
	}

	/** Checks whether the result is an MOD_RES_ALLOW or MOD_RES_PASSTHRU when the default is to allow. */
	inline bool check(bool def) const
	{
		return (result == 1 || (result == 0 && def));
	}

	/* Merges two results preferring MOD_RES_ALLOW to MOD_RES_DENY. */
	inline ModResult operator+(const ModResult& res) const
	{
		// If the results are identical or the other result is MOD_RES_PASSTHRU
		// then return this result.
		if (result == res.result || res.result == 0)
			return *this;

		// If this result is MOD_RES_PASSTHRU then return the other result.
		if (result == 0)
			return res;

		// Otherwise, they are different, and neither is MOD_RES_PASSTHRU.
		return MOD_RES_ALLOW;
	}
};

/** Priority types which can be used by Module::Prioritize()
 */
enum Priority { PRIORITY_FIRST, PRIORITY_LAST, PRIORITY_BEFORE, PRIORITY_AFTER };

/** Implementation-specific flags which may be set in Module::Implements()
 */
enum Implementation
{
	I_OnAcceptConnection,
	I_OnAddLine,
	I_OnBackgroundTimer,
	I_OnBuildNeighborList,
	I_OnChangeConnectClass,
	I_OnChangeHost,
	I_OnChangeRealHost,
	I_OnChangeRealName,
	I_OnChangeRealUser,
	I_OnChangeRemoteAddress,
	I_OnChangeUser,
	I_OnChannelDelete,
	I_OnChannelPreDelete,
	I_OnCheckBan,
	I_OnCheckChannelBan,
	I_OnCheckInvite,
	I_OnCheckKey,
	I_OnCheckLimit,
	I_OnCheckPassword,
	I_OnCheckReady,
	I_OnCommandBlocked,
	I_OnDecodeMetadata,
	I_OnDelLine,
	I_OnExpireLine,
	I_OnGarbageCollect,
	I_OnKill,
	I_OnLoadModule,
	I_OnMode,
	I_OnModuleRehash,
	I_OnNumeric,
	I_OnOperLogin,
	I_OnOperLogout,
	I_OnPostChangeConnectClass,
	I_OnPostChangeRealHost,
	I_OnPostChangeRealUser,
	I_OnPostCommand,
	I_OnPostConnect,
	I_OnPostJoin,
	I_OnPostOperLogin,
	I_OnPostOperLogout,
	I_OnPostTopicChange,
	I_OnPreChangeConnectClass,
	I_OnPreCommand,
	I_OnPreMode,
	I_OnPreOperLogin,
	I_OnPreRehash,
	I_OnPreTopicChange,
	I_OnRawMode,
	I_OnSendSnotice,
	I_OnServiceAdd,
	I_OnServiceDel,
	I_OnShutdown,
	I_OnUnloadModule,
	I_OnUserConnect,
	I_OnUserDisconnect,
	I_OnUserInit,
	I_OnUserInvite,
	I_OnUserJoin,
	I_OnUserKick,
	I_OnUserMessage,
	I_OnUserMessageBlocked,
	I_OnUserPart,
	I_OnUserPostInit,
	I_OnUserPostMessage,
	I_OnUserPostNick,
	I_OnUserPreInvite,
	I_OnUserPreJoin,
	I_OnUserPreKick,
	I_OnUserPreMessage,
	I_OnUserPreNick,
	I_OnUserPreQuit,
	I_OnUserQuit,
	I_OnUserRegister,
	I_OnUserWrite,
	I_END
};

/** Base class for all InspIRCd modules
 *  This class is the base class for InspIRCd modules. All modules must inherit from this class,
 *  its methods will be called when irc server events occur. class inherited from module must be
 *  instantiated by the ModuleFactory class (see relevant section) for the module to be initialised.
 */
class CoreExport Module
	: public Cullable
	, public usecountbase
{
protected:
	/** Initializes a new instance of the Module class.
	 * @param mprops The properties of this module.
	 * @param mdesc A description of this module.
	 */
	Module(int mprops, const std::string& mdesc);

	/** Initializes a new instance of the Module class.
	 * @param mprops The properties of this module.
	 * @param mversion The version of this module (for contrib modules).
	 * @param mdesc A description of this module.
	 */
	Module(int mprops, const std::string& mversion, const std::string& mdesc);

	/** Detach an event from this module
	 * @param i Event type to detach
	 */
	void DetachEvent(Implementation i);

public:
	/** Data which is synchronised between servers on link. */
	typedef std::map<std::string, std::string, irc::insensitive_swo> LinkData;

	/** Data which is synchronised between servers on link. */
	typedef std::map<std::string, std::pair<std::optional<std::string>, std::optional<std::string>>, irc::insensitive_swo> LinkDataDiff;

	/** A list of modules. */
	typedef std::vector<Module*> List;

	/** Reference to the dynamic library. */
	DLLManager* ModuleDLL = nullptr;

	/** File that this module was loaded from. */
	std::string ModuleFile;

	/** If true, this module will be unloaded soon, further unload attempts will fail
	 * Value is used by the ModuleManager internally, you should not modify it
	 */
	bool dying = false;

	/** A description of this module. */
	const std::string description;

	/** The properties of this module. */
	const int properties;

	/** The version of this module. */
	const std::string version;

	/** Module setup
	 * \exception ModuleException Throwing this class, or any class derived from ModuleException, causes loading of the module to abort.
	 */
	virtual void init() { }

	/** Clean up prior to destruction
	 * If you override, you must call this AFTER your module's cleanup
	 */
	Cullable::Result Cull() override;

	/** Compares our link data to that of another server.
	 * @param otherdata Link data from another server.
	 * @param diffs The difference between the two sets of link data.
	 */
	virtual void CompareLinkData(const LinkData& otherdata, LinkDataDiff& diffs);

	/** Retrieves link compatibility data for this module.
	 * @param data The location to store link compatibility data.
	 */
	virtual void GetLinkData(LinkData& data);

	/** Retrieves a string that represents the properties of this module. */
	std::string GetPropertyString() const;

	/** Retrieves the version of the module. */
	std::string GetVersion() const;

	/** This method is called when you should reload module specific configuration:
	 * on boot, on a /REHASH and on module load.
	 * @param status The current status, can be inspected for more information.
	 */
	virtual void ReadConfig(ConfigStatus& status);

	/** Called when the hooks provided by a module need to be prioritised. */
	virtual void Prioritize();

	/** Called when a user connects.
	 * The details of the connecting user are available to you in the parameter User* user
	 * @param user The user who is connecting
	 */
	virtual void OnUserConnect(LocalUser* user) ATTR_NOT_NULL(2);

	/** Called when before a user quits.
	 * The details of the exiting user are available to you in the parameter User* user
	 * This event is only called when the user is fully connected when they quit. To catch
	 * raw disconnections, use the OnUserDisconnect method.
	 * @param user The user who is quitting
	 * @param message The user's quit message (as seen by non-opers)
	 * @param oper_message The user's quit message (as seen by opers)
	 */
	virtual ModResult OnUserPreQuit(LocalUser* user, std::string& message, std::string& oper_message) ATTR_NOT_NULL(2);

	/** Called when a user quits.
	 * The details of the exiting user are available to you in the parameter User* user
	 * This event is only called when the user is fully connected when they quit. To catch
	 * raw disconnections, use the OnUserDisconnect method.
	 * @param user The user who is quitting
	 * @param message The user's quit message (as seen by non-opers)
	 * @param oper_message The user's quit message (as seen by opers)
	 */
	virtual void OnUserQuit(User* user, const std::string& message, const std::string& oper_message) ATTR_NOT_NULL(2);

	/** Called whenever a user's socket is closed.
	 * The details of the exiting user are available to you in the parameter User* user
	 * This event is called for all users, registered or not, as a cleanup method for modules
	 * which might assign resources to user, such as dns lookups, objects and sockets.
	 * @param user The user who is disconnecting
	 */
	virtual void OnUserDisconnect(LocalUser* user) ATTR_NOT_NULL(2);

	/** Called whenever a channel is about to be deleted
	 * @param chan The channel being deleted
	 * @return An integer specifying whether or not the channel may be deleted. 0 for yes, 1 for no.
	 */
	virtual ModResult OnChannelPreDelete(Channel* chan) ATTR_NOT_NULL(2);

	/** Called whenever a channel is deleted, either by QUIT, KICK or PART.
	 * @param chan The channel being deleted
	 */
	virtual void OnChannelDelete(Channel* chan) ATTR_NOT_NULL(2);

	/** Called when a user joins a channel.
	 * The details of the joining user are available to you in the parameter User* user,
	 * and the details of the channel they have joined is available in the variable Channel* channel
	 * @param memb The channel membership being created
	 * @param sync This is set to true if the JOIN is the result of a network sync and the remote user is being introduced
	 * to a channel due to the network sync.
	 * @param created This is true if the join created the channel
	 * @param except_list A list of users not to send to.
	 */
	virtual void OnUserJoin(Membership* memb, bool sync, bool created, CUList& except_list) ATTR_NOT_NULL(2);

	/** Called after a user joins a channel
	 * Identical to OnUserJoin, but called immediately afterwards, when any linking module has
	 * seen the join.
	 * @param memb The channel membership created
	 */
	virtual void OnPostJoin(Membership* memb) ATTR_NOT_NULL(2);

	/** Called when a user parts a channel.
	 * The details of the leaving user are available to you in the parameter User* user,
	 * and the details of the channel they have left is available in the variable Channel* channel
	 * @param memb The channel membership being destroyed
	 * @param partmessage The part message, or an empty string (may be modified)
	 * @param except_list A list of users to not send to.
	 */
	virtual void OnUserPart(Membership* memb, std::string& partmessage, CUList& except_list) ATTR_NOT_NULL(2);

	/** Called on rehash.
	 * This method is called prior to a /REHASH or when a SIGHUP is received from the operating
	 * system. This is called in all cases -- including when this server will not execute the
	 * rehash because it is directed at a remote server.
	 *
	 * @param user The user performing the rehash, if any. If this is server initiated, the value of
	 * this variable will be NULL.
	 * @param parameter The (optional) parameter given to REHASH from the user. Empty when server
	 * initiated.
	 */
	virtual void OnPreRehash(User* user, const std::string& parameter);

	/** Called on rehash.
	 * This method is called when a user initiates a module-specific rehash. This can be used to do
	 * expensive operations (such as reloading TLS certificates) that are not executed on a normal
	 * rehash for efficiency. A rehash of this type does not reload the core configuration.
	 *
	 * @param user The user performing the rehash.
	 * @param parameter The parameter given to REHASH
	 */
	virtual void OnModuleRehash(User* user, const std::string& parameter);

	/** Called whenever a snotice is about to be sent to a snomask.
	 * snomask and type may both be modified; the message may not.
	 * @param snomask The snomask the message is going to (e.g. 'A')
	 * @param type The textual description the snomask will go to (e.g. 'OPER')
	 * @param message The text message to be sent via snotice
	 * @return 1 to block the snotice from being sent entirely, 0 else.
	 */
	virtual ModResult OnSendSnotice(char& snomask, std::string& type, const std::string& message);

	/** Called whenever a user is about to join a channel, before any processing is done.
	 * Returning a value of 1 from this function stops the process immediately, causing no
	 * output to be sent to the user by the core. If you do this you must produce your own numerics,
	 * notices etc. This is useful for modules which may want to mimic +b, +k, +l etc. Returning -1 from
	 * this function forces the join to be allowed, bypassing restrictions such as banlists, invite, keys etc.
	 *
	 * IMPORTANT NOTE!
	 *
	 * If the user joins a NEW channel which does not exist yet, OnUserPreJoin will be called BEFORE the channel
	 * record is created. This will cause Channel* chan to be NULL. There is very little you can do in form of
	 * processing on the actual channel record at this point, however the channel NAME will still be passed in
	 * char* cname, so that you could for example implement a channel blacklist or whitelist, etc.
	 * @param user The user joining the channel
	 * @param chan If the  channel is a new channel, this will be NULL, otherwise it will be a pointer to the channel being joined
	 * @param cname The channel name being joined. For new channels this is valid where chan is not.
	 * @param privs A string containing the users privileges when joining the channel. For new channels this will contain "o".
	 * You may alter this string to alter the user's modes on the channel.
	 * @param keygiven The key given to join the channel, or an empty string if none was provided
	 * @param override Whether the channel join can be blocked by returning MOD_RES_DENY.
	 * @return 1 To prevent the join, 0 to allow it.
	 */
	virtual ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven, bool override) ATTR_NOT_NULL(2);

	/** Called whenever a user is about to be kicked.
	 * Returning a value of 1 from this function stops the process immediately, causing no
	 * output to be sent to the user by the core. If you do this you must produce your own numerics,
	 * notices etc.
	 * @param source The user issuing the kick
	 * @param memb The channel membership of the user who is being kicked.
	 * @param reason The kick reason
	 * @return 1 to prevent the kick, 0 to continue normally, -1 to explicitly allow the kick regardless of normal operation
	 */
	virtual ModResult OnUserPreKick(User* source, Membership* memb, const std::string& reason)  ATTR_NOT_NULL(2, 3);

	/** Called whenever a user is kicked.
	 * If this method is called, the kick is already underway and cannot be prevented, so
	 * to prevent a kick, please use Module::OnUserPreKick instead of this method.
	 * @param source The user issuing the kick
	 * @param memb The channel membership of the user who was kicked.
	 * @param reason The kick reason
	 * @param except_list A list of users to not send to.
	 */
	virtual void OnUserKick(User* source, Membership* memb, const std::string& reason, CUList& except_list) ATTR_NOT_NULL(2, 3);

	/** Called whenever a user is about to invite another user into a channel, before any processing is done.
	 * Returning 1 from this function stops the process immediately, causing no
	 * output to be sent to the user by the core. If you do this you must produce your own numerics,
	 * notices etc. This is useful for modules which may want to filter invites to channels.
	 * @param source The user who is issuing the INVITE
	 * @param dest The user being invited
	 * @param channel The channel the user is being invited to
	 * @param timeout The time the invite will expire (0 == never)
	 * @return 1 to deny the invite, 0 to check whether or not the user has permission to invite, -1 to explicitly allow the invite
	 */
	virtual ModResult OnUserPreInvite(User* source, User* dest, Channel* channel, time_t timeout) ATTR_NOT_NULL(2, 3, 4);

	/** Called after a user has been successfully invited to a channel.
	 * You cannot prevent the invite from occurring using this function, to do that,
	 * use OnUserPreInvite instead.
	 * @param source The user who is issuing the INVITE
	 * @param dest The user being invited
	 * @param channel The channel the user is being invited to
	 * @param timeout The time the invite will expire (0 == never)
	 * @param notifyrank Rank required to get an invite announcement (if enabled)
	 * @param notifyexcepts List of users to not send the default NOTICE invite announcement to
	 */
	virtual void OnUserInvite(User* source, User* dest, Channel* channel, time_t timeout, ModeHandler::Rank notifyrank, CUList& notifyexcepts)  ATTR_NOT_NULL(2, 3, 4);

	/** Called before a user sends a message to a channel, a user, or a server glob mask.
	 * @param user The user sending the message.
	 * @param target The target of the message. This can either be a channel, a user, or a server
	 *               glob mask.
	 * @param details Details about the message such as the message text and type. See the
	 *                MessageDetails class for more information.
	 * @return MOD_RES_ALLOW to explicitly allow the message, MOD_RES_DENY to explicitly deny the
	 *         message, or MOD_RES_PASSTHRU to let another module handle the event.
	 */
	virtual ModResult OnUserPreMessage(User* user, MessageTarget& target, MessageDetails& details) ATTR_NOT_NULL(2);

	/** Called when sending a message to all "neighbors" of a given user -
	 * that is, all users that share a common channel. This is used in
	 * commands such as NICK, QUIT, etc.
	 * @param source The source of the message
	 * @param include Memberships to scan for users to include
	 * @param exceptions Map of user->bool that overrides the inclusion decision
	 *
	 * Set exceptions[user] = true to include, exceptions[user] = false to exclude
	 */
	virtual void OnBuildNeighborList(User* source, User::NeighborList& include, User::NeighborExceptions& exceptions) ATTR_NOT_NULL(2);

	/** Called before local nickname changes. This can be used to implement Q-lines etc.
	 * If your method returns nonzero, the nickchange is silently forbidden, and it is down to your
	 * module to generate some meaningful output.
	 * @param user The username changing their nick
	 * @param newnick Their new nickname
	 * @return 1 to deny the change, 0 to allow
	 */
	virtual ModResult OnUserPreNick(LocalUser* user, const std::string& newnick) ATTR_NOT_NULL(2);

	/** Called immediately after a user sends a message to a channel, a user, or a server glob mask.
	 * @param user The user sending the message.
	 * @param target The target of the message. This can either be a channel, a user, or a server
	 *               glob mask.
	 * @param details Details about the message such as the message text and type. See the
	 *                MessageDetails class for more information.
	 */
	virtual void OnUserPostMessage(User* user, const MessageTarget& target, const MessageDetails& details) ATTR_NOT_NULL(2);

	/** Called immediately before a user sends a message to a channel, a user, or a server glob mask.
	 * @param user The user sending the message.
	 * @param target The target of the message. This can either be a channel, a user, or a server
	 *               glob mask.
	 * @param details Details about the message such as the message text and type. See the
	 *                MessageDetails class for more information.
	 */
	virtual void OnUserMessage(User* user, const MessageTarget& target, const MessageDetails& details) ATTR_NOT_NULL(2);

	/** Called when a message sent by a user to a channel, a user, or a server glob mask is blocked.
	 * @param user The user sending the message.
	 * @param target The target of the message. This can either be a channel, a user, or a server
	 *               glob mask.
	 * @param details Details about the message such as the message text and type. See the
	 *                MessageDetails class for more information.
	 */
	virtual void OnUserMessageBlocked(User* user, const MessageTarget& target, const MessageDetails& details) ATTR_NOT_NULL(2);

	/** Called after every MODE command sent from a user
	 * Either the usertarget or the chantarget variable contains the target of the modes,
	 * the actual target will have a non-NULL pointer.
	 * All changed modes are available in the changelist object.
	 * @param user The user sending the MODEs
	 * @param usertarget The target user of the modes, NULL if the target is a channel
	 * @param chantarget The target channel of the modes, NULL if the target is a user
	 * @param changelist The changed modes.
	 * @param processflags Flags passed to ModeParser::Process(), see ModeParser::ModeProcessFlags
	 * for the possible flags.
	 */
	virtual void OnMode(User* user, User* usertarget, Channel* chantarget, const Modes::ChangeList& changelist, ModeParser::ModeProcessFlag processflags) ATTR_NOT_NULL(2);

	/** Allows module data, sent via SendMetadata, to be decoded again by a receiving module.
	 * @param target The Channel* or User* that data should be added to
	 * @param extname The extension name which is being sent
	 * @param extdata The extension data, encoded at the other end by an identical module
	 */
	virtual void OnDecodeMetadata(Extensible* target, const std::string& extname, const std::string& extdata);

	/** Called whenever a user's hostname is changed.
	 * This event triggers after the host has been set.
	 * @param user The user whose host is being changed
	 * @param newhost The new hostname being set
	 */
	virtual void OnChangeHost(User* user, const std::string& newhost) ATTR_NOT_NULL(2);

	/** Called whenever a user's real hostname is changed.
	 * This event triggers before the host has been set.
	 * @param user The user whose host is being changed
	 * @param newhost The new hostname being set
	 */
	virtual void OnChangeRealHost(User* user, const std::string& newhost) ATTR_NOT_NULL(2);

	/** Called whenever a user's real hostname is changed.
	 * This event triggers after the host has been set.
	 * @param user The user whos host was changed.
	 */
	virtual void OnPostChangeRealHost(User* user) ATTR_NOT_NULL(2);

	/** Called whenever a user's real name is changed.
	 * This event triggers after the name has been set.
	 * @param user The user who's real name is being changed
	 * @param real The new real name being set on the user
	 */
	virtual void OnChangeRealName(User* user, const std::string& real) ATTR_NOT_NULL(2);

	/** Called before a user's username is changed.
	 * @param user The user who's username is being changed
	 * @param newuser The new username being set.
	 */
	virtual void OnChangeUser(User* user, const std::string& newuser) ATTR_NOT_NULL(2);

	/** Called before a user's real username is changed.
	 * @param user The user who's real username is being changed
	 * @param newuser The new real username being set.
	 */
	virtual void OnChangeRealUser(User* user, const std::string& newuser) ATTR_NOT_NULL(2);

	/** Called after a user's real username is changed.
	 * @param user The user whos real username was changed.
	 */
	virtual void OnPostChangeRealUser(User* user) ATTR_NOT_NULL(2);

	/** Called whenever an xline is added by a local user.
	 * This method is triggered after the line is added.
	 * @param source The sender of the line or NULL for local server
	 * @param line The xline being added
	 */
	virtual void OnAddLine(User* source, XLine* line) ATTR_NOT_NULL(3);

	/** Called whenever an xline is deleted MANUALLY. See OnExpireLine for expiry.
	 * This method is triggered after the line is deleted.
	 * @param source The user removing the line or NULL for local server
	 * @param line the line being deleted
	 */
	virtual void OnDelLine(User* source, XLine* line) ATTR_NOT_NULL(3);

	/** Called whenever an xline expires.
	 * This method is triggered after the line is deleted.
	 * @param line The line being deleted.
	 */
	virtual void OnExpireLine(XLine* line) ATTR_NOT_NULL(2);

	/** Called before the module is unloaded to clean up extension.
	 * This method is called once for every channel, membership, and user.
	 * so that you can clear up any data relating to the specified extensible.
	 * @param type The type of extensible being cleaned up.
	 * @param item A pointer to the extensible which is being cleaned up.
	 */
	virtual void OnCleanup(ExtensionType type, Extensible* item) ATTR_NOT_NULL(3);

	/** Called after any nickchange, local or remote. This can be used to track users after nickchanges
	 * have been applied. Please note that although you can see remote nickchanges through this function, you should
	 * NOT make any changes to the User if the user is a remote user as this may cause a desync.
	 * check user->server before taking any action (including returning nonzero from the method).
	 * Because this method is called after the nickchange is taken place, no return values are possible
	 * to indicate forbidding of the nick change. Use OnUserPreNick for this.
	 * @param user The user changing their nick
	 * @param oldnick The old nickname of the user before the nickchange
	 */
	virtual void OnUserPostNick(User* user, const std::string& oldnick) ATTR_NOT_NULL(2);

	/** Called before a mode change via the MODE command, to allow a single access check for
	 * a full mode change (use OnRawMode to check individual modes)
	 *
	 * Returning MOD_RES_ALLOW will skip prefix level checks, but can be overridden by
	 * OnRawMode for each individual mode
	 *
	 * @param source the user making the mode change
	 * @param dest the user destination of the umode change (NULL if a channel mode)
	 * @param channel the channel destination of the mode change
	 * @param modes Modes being changed, can be edited
	 */
	virtual ModResult OnPreMode(User* source, User* dest, Channel* channel, Modes::ChangeList& modes) ATTR_NOT_NULL(2);

	/** Called when a client is disconnected by KILL.
	 * If a client is killed by a server, e.g. a nickname collision or protocol error,
	 * source is NULL.
	 * Return 1 from this function to prevent the kill, and 0 from this function to allow
	 * it as normal. If you prevent the kill no output will be sent to the client, it is
	 * down to your module to generate this information.
	 * NOTE: It is NOT advisable to stop kills which originate from servers or remote users.
	 * If you do so youre risking race conditions, desyncs and worse!
	 * @param source The user sending the KILL
	 * @param dest The user being killed
	 * @param reason The kill reason
	 * @return 1 to prevent the kill, 0 to allow
	 */
	virtual ModResult OnKill(User* source, User* dest, const std::string& reason) ATTR_NOT_NULL(2, 3);

	/** Called whenever a module is loaded.
	 * mod will contain a pointer to the module, and string will contain its name,
	 * for example m_widgets.so. This function is primary for dependency checking,
	 * your module may decide to enable some extra features if it sees that you have
	 * for example loaded "m_killwidgets.so" with "m_makewidgets.so". It is highly
	 * recommended that modules do *NOT* bail if they cannot satisfy dependencies,
	 * but instead operate under reduced functionality, unless the dependency is
	 * absolutely necessary (e.g. a module that extends the features of another
	 * module).
	 * @param mod A pointer to the new module
	 */
	virtual void OnLoadModule(Module* mod) ATTR_NOT_NULL(2);

	/** Called whenever a module is unloaded.
	 * mod will contain a pointer to the module, and string will contain its name,
	 * for example m_widgets.so. This function is primary for dependency checking,
	 * your module may decide to enable some extra features if it sees that you have
	 * for example loaded "m_killwidgets.so" with "m_makewidgets.so". It is highly
	 * recommended that modules do *NOT* bail if they cannot satisfy dependencies,
	 * but instead operate under reduced functionality, unless the dependency is
	 * absolutely necessary (e.g. a module that extends the features of another
	 * module).
	 * @param mod Pointer to the module being unloaded (still valid)
	 */
	virtual void OnUnloadModule(Module* mod) ATTR_NOT_NULL(2);

	/** Called once every five seconds for background processing.
	 * This timer can be used to control timed features. Its period is not accurate
	 * enough to be used as a clock, but it is guaranteed to be called at least once in
	 * any five second period, directly from the main loop of the server.
	 * @param curtime The current timer derived from time(2)
	 */
	virtual void OnBackgroundTimer(time_t curtime);

	/** Called whenever any command is about to be executed.
	 * This event occurs for all registered commands, whether they are registered in the core,
	 * or another module, and for invalid commands. Invalid commands may only be sent to this
	 * function when the value of validated is false. By returning 1 from this method you may prevent the
	 * command being executed. If you do this, no output is created by the core, and it is
	 * down to your module to produce any output necessary.
	 * Note that unless you return 1, you should not destroy any structures (e.g. by using
	 * InspIRCd::QuitUser) otherwise when the command's handler function executes after your
	 * method returns, it will be passed an invalid pointer to the user object and crash!)
	 * @param command The command being executed
	 * @param parameters An array of array of characters containing the parameters for the command
	 * @param user the user issuing the command
	 * @param validated True if the command has passed all checks, e.g. it is recognised, has enough parameters, the user has permission to execute it, etc.
	 * You should only change the parameter list and command string if validated == false (e.g. before the command lookup occurs).
	 * @return 1 to block the command, 0 to allow
	 */
	virtual ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) ATTR_NOT_NULL(4);

	/** Called after any command has been executed.
	 * This event occurs for all registered commands, whether they are registered in the core,
	 * or another module, but it will not occur for invalid commands (e.g. ones which do not
	 * exist within the command table). The result code returned by the command handler is
	 * provided.
	 * @param command The command being executed
	 * @param parameters An array of array of characters containing the parameters for the command
	 * @param user the user issuing the command
	 * @param result The return code given by the command handler, one of CmdResult::SUCCESS or CmdResult::FAILURE
	 * @param loop Whether the command is being called from LoopCall or directly.
	 */
	virtual void OnPostCommand(Command* command, const CommandBase::Params& parameters, LocalUser* user, CmdResult result, bool loop) ATTR_NOT_NULL(2, 4);

	/** Called when a command was blocked before it could be executed.
	 * @param command The command being executed.
	 * @param parameters The parameters for the command.
	 * @param user The user issuing the command.
	 */
	virtual void OnCommandBlocked(const std::string& command, const CommandBase::Params& parameters, LocalUser* user) ATTR_NOT_NULL(4);

	/** Called after a user object is initialised and added to the user list.
	 * When this is called the user has not had their I/O hooks checked or had their initial
	 * connect class assigned and may not yet have a serializer. You probably want to use
	 * the OnUserPostInit or OnUserSetIP hooks instead of this one.
	 * @param user The connecting user.
	 */
	virtual void OnUserInit(LocalUser* user) ATTR_NOT_NULL(2);

	/** Called after a user object has had their I/O hooks checked, their initial connection
	 * class assigned, and had a serializer set.
	 * @param user The connecting user.
	 */
	virtual void OnUserPostInit(LocalUser* user) ATTR_NOT_NULL(2);

	/** Called to check if a user who is connecting can now be allowed to register
	 * If any modules return false for this function, the user is held in the waiting
	 * state until all modules return true. For example a module which implements ident
	 * lookups will continue to return false for a user until their ident lookup is completed.
	 * Note that the connection timeout for a user overrides these checks, if the connection
	 * timeout is reached, the user is disconnected even if modules report that the user is
	 * not ready to connect.
	 * @param user The user to check
	 * @return true to indicate readiness, false if otherwise
	 */
	virtual ModResult OnCheckReady(LocalUser* user) ATTR_NOT_NULL(2);

	/** Called whenever a user is about to register their connection (e.g. before the user
	 * is sent the MOTD etc). Modules can use this method if they are performing a function
	 * which must be done before the actual connection is completed (e.g. ident lookups,
	 * dnsbl lookups, etc).
	 * Note that you should NOT delete the user record here by causing a disconnection!
	 * Use OnUserConnect for that instead.
	 * @param user The user registering
	 * @return 1 to indicate user quit, 0 to continue
	 */
	virtual ModResult OnUserRegister(LocalUser* user) ATTR_NOT_NULL(2);

	/** Called whenever a user joins a channel, to determine if invite checks should go ahead or not.
	 * This method will always be called for each join, whether or not the channel is actually +i, and
	 * determines the outcome of an if statement around the whole section of invite checking code.
	 * return 1 to explicitly allow the join to go ahead or 0 to ignore the event.
	 * @param user The user joining the channel
	 * @param chan The channel being joined
	 * @return 1 to explicitly allow the join, 0 to proceed as normal
	 */
	virtual ModResult OnCheckInvite(User* user, Channel* chan) ATTR_NOT_NULL(2, 3);

	/** Called whenever a mode character is processed.
	 * Return 1 from this function to block the mode character from being processed entirely.
	 * @param user The user who is sending the mode
	 * @param chan The channel the mode is being sent to (or NULL if a usermode)
	 * @param change Information regarding the mode change.
	 * @return MOD_RES_DENY to deny the mode, MOD_RES_DEFAULT to do standard mode checking, and MOD_RES_ALLOW
	 * to skip all permission checking. Please note that for remote mode changes, your return value
	 * will be ignored!
	 */
	virtual ModResult OnRawMode(User* user, Channel* chan, const Modes::Change& change) ATTR_NOT_NULL(2);

	/** Called whenever a user joins a channel, to determine if key checks should go ahead or not.
	 * This method will always be called for each join, whether or not the channel is actually +k, and
	 * determines the outcome of an if statement around the whole section of key checking code.
	 * if the user specified no key, the keygiven string will be a valid but empty value.
	 * return 1 to explicitly allow the join to go ahead or 0 to ignore the event.
	 * @param user The user joining the channel
	 * @param chan The channel being joined
	 * @param keygiven The key given on joining the channel.
	 * @return 1 to explicitly allow the join, 0 to proceed as normal
	 */
	virtual ModResult OnCheckKey(User* user, Channel* chan, const std::string& keygiven) ATTR_NOT_NULL(2, 3);

	/** Called whenever a user joins a channel, to determine if channel limit checks should go ahead or not.
	 * This method will always be called for each join, whether or not the channel is actually +l, and
	 * determines the outcome of an if statement around the whole section of channel limit checking code.
	 * return 1 to explicitly allow the join to go ahead or 0 to ignore the event.
	 * @param user The user joining the channel
	 * @param chan The channel being joined
	 * @return 1 to explicitly allow the join, 0 to proceed as normal
	 */
	virtual ModResult OnCheckLimit(User* user, Channel* chan) ATTR_NOT_NULL(2, 3);

	/**
	 * Checks for a user's ban from the channel
	 * @param user The user to check
	 * @param chan The channel to check in
	 * @return MOD_RES_DENY to mark as banned, MOD_RES_ALLOW to skip the
	 * ban check, or MOD_RES_PASSTHRU to check bans normally
	 */
	virtual ModResult OnCheckChannelBan(User* user, Channel* chan) ATTR_NOT_NULL(2, 3);

	/**
	 * Checks for a user's match of a single ban
	 * @param user The user to check for match
	 * @param chan The channel on which the match is being checked
	 * @param mask The mask being checked
	 * @return MOD_RES_DENY to mark as banned, MOD_RES_ALLOW to skip the
	 * ban check, or MOD_RES_PASSTHRU to check bans normally
	 */
	virtual ModResult OnCheckBan(User* user, Channel* chan, const std::string& mask)  ATTR_NOT_NULL(2, 3);

	/** Called when checking if a password is valid.
	 * @param password The hashed password.
	 * @param passwordhash The name of the algorithm used to hash the password.
	 * @param value The value to check to see if the password is valid.
	 * @return MOD_RES_ALLOW if the password is correct, MOD_RES_DENY if the password is incorrect,
	 * or MOD_RES_PASSTHRU to let another module handle the event.
	 */
	virtual ModResult OnCheckPassword(const std::string& password, const std::string& passwordhash, const std::string& value);

	/** Called before a topic is changed.
	 * Return 1 to deny the topic change, 0 to check details on the change, -1 to let it through with no checks
	 * As with other 'pre' events, you should only ever block a local event.
	 * @param user The user changing the topic
	 * @param chan The channels who's topic is being changed
	 * @param topic The actual topic text
	 * @return 1 to block the topic change, 0 to allow
	 */
	virtual ModResult OnPreTopicChange(User* user, Channel* chan, const std::string& topic) ATTR_NOT_NULL(2, 3);

	/** Called whenever a topic has been changed.
	 * To block topic changes you must use OnPreTopicChange instead.
	 * @param user The user changing the topic
	 * @param chan The channels who's topic is being changed
	 * @param topic The actual topic text
	 */
	virtual void OnPostTopicChange(User* user, Channel* chan, const std::string& topic) ATTR_NOT_NULL(2, 3);

	/** Called after a user has fully connected and all modules have executed OnUserConnect
	 * This event is informational only. You should not change any user information in this
	 * event. To do so, use the OnUserConnect method to change the state of local users.
	 * This is called for both local and remote users.
	 * @param user The user who is connecting
	 */
	virtual void OnPostConnect(User* user)  ATTR_NOT_NULL(2);

	/** Called when a port accepts a connection
	 * Return MOD_RES_ACCEPT if you have used the file descriptor.
	 * @param fd The file descriptor returned from accept()
	 * @param sock The socket connection for the new user
	 * @param client The client IP address and port
	 * @param server The server IP address and port
	 */
	virtual ModResult OnAcceptConnection(int fd, ListenSocket* sock, const irc::sockets::sockaddrs& client, const irc::sockets::sockaddrs& server) ATTR_NOT_NULL(3);

	/** Called at intervals for modules to garbage-collect any hashes etc.
	 * Certain data types such as hash_map 'leak' buckets, which must be
	 * tidied up and freed by copying into a new item every so often. This
	 * method is called when it is time to do that.
	 */
	virtual void OnGarbageCollect();

	virtual ModResult OnNumeric(User* user, const Numeric::Numeric& numeric) ATTR_NOT_NULL(2);

	/** Called whenever a local user's remote address is set or changed.
	 * @param user The user whose remote address is being set or changed.
	 */
	virtual void OnChangeRemoteAddress(LocalUser* user) ATTR_NOT_NULL(2);

	/** Called whenever a ServiceProvider is registered.
	 * @param service ServiceProvider being registered.
	 */
	virtual void OnServiceAdd(ServiceProvider& service);

	/** Called whenever a ServiceProvider is unregistered.
	 * @param service ServiceProvider being unregistered.
	 */
	virtual void OnServiceDel(ServiceProvider& service);

	/** Called whenever a message is about to be written to a user.
	 * @param user The user who is having a message sent to them.
	 * @param msg The message which is being written to the user.
	 * @return MOD_RES_ALLOW to explicitly allow the message to be sent, MOD_RES_DENY to explicitly
	 * deny the message from being sent, or MOD_RES_PASSTHRU to let another module handle the event.
	 */
	virtual ModResult OnUserWrite(LocalUser* user, ClientProtocol::Message& msg) ATTR_NOT_NULL(2);

	/** Called before a server shuts down.
	 * @param reason The reason the server is shutting down.
	 */
	virtual void OnShutdown(const std::string& reason);

	/** Called when a local user is attempting to log in to an server operator account.
	 * @param user The user who is attempting to log in.
	 * @param oper The server operator account they are attempting to log in to.
	 * @param automatic Whether the login attempt is being performed automatically.
	 * @return MOD_RES_ALLOW to explicitly allow the login, MOD_RES_DENY to explicitly deny the
	 *         login, or MOD_RES_PASSTHRU to let another module handle the event.
	 */
	virtual ModResult OnPreOperLogin(LocalUser* user, const std::shared_ptr<OperAccount>& oper, bool automatic) ATTR_NOT_NULL(2);

	/** Called when a user is about to be logged in to an server operator account.
	 * @param user The user who is about to be logged in.
	 * @param oper The server operator account they are logging in to.
	 * @param automatic Whether the login was performed automatically.
	 */
	virtual void OnOperLogin(User* user, const std::shared_ptr<OperAccount>& oper, bool automatic) ATTR_NOT_NULL(2);

	/** Called after a user has been logged in to an server operator account.
	 * @param user The user who has been logged in.
	 * @param automatic Whether the login was performed automatically.
	 */
	virtual void OnPostOperLogin(User* user, bool automatic) ATTR_NOT_NULL(2);

	/** Called when a user is about to be logged out of an server operator account.
	 * @param user The user who is about to be logged out.
	 */
	virtual void OnOperLogout(User* user) ATTR_NOT_NULL(2);

	/** Called after a user has been logged out of an server operator account.
	 * @param user The user who has been logged out.
	 * @param oper The server operator account they were logged out of.
	 */
	virtual void OnPostOperLogout(User* user, const std::shared_ptr<OperAccount>& oper) ATTR_NOT_NULL(2);

	/** Called when trying to find a connect class for a user.
	 * @param user The user that needs a new connect class.
	 * @param klass The connect class to check the suitability of.
	 * @param errnum If returning MOD_RES_DENY then the error numeric to send to the user.
	 * @return MOD_RES_ALLOW to select this connect class, MOD_RES_DENY to reject this connect
	 *         class, or MOD_RES_PASSTHRU to let another module handle the event.
	 */
	virtual ModResult OnPreChangeConnectClass(LocalUser* user, const std::shared_ptr<ConnectClass>& klass, std::optional<Numeric::Numeric>& errnum) ATTR_NOT_NULL(2);

	/** Called when a user is about to be assigned to a connect class.
	 * @param user The user that is being assigned to a connect class.
	 * @param klass The connect class the user is being assigned to.
	 * @param force Whether the connect class was explicitly picked (e.g. via <oper:class>).
	 */
	virtual void OnChangeConnectClass(LocalUser* user, const std::shared_ptr<ConnectClass>& klass, bool force) ATTR_NOT_NULL(2);

	/** Called when a user has bee assigned to a connect class.
	 * @param user The user that was assigned to a connect class.
	 * @param force Whether the connect class was explicitly picked (e.g. via <oper:class>).
	 */
	virtual void OnPostChangeConnectClass(LocalUser* user, bool force) ATTR_NOT_NULL(2);
};

/** ModuleManager takes care of all things module-related
 * in the core.
 */
class CoreExport ModuleManager final
{
public:
	typedef std::multimap<std::string, ServiceProvider*, irc::insensitive_swo> DataProviderMap;
	typedef std::vector<ServiceProvider*> ServiceList;

private:
	/** Holds a string describing the last module error to occur
	 */
	std::string LastModuleError;

	/** List of loaded modules and shared object/dll handles
	 * keyed by module name
	 */
	std::map<std::string, Module*> Modules;

	enum {
		PRIO_STATE_FIRST,
		PRIO_STATE_AGAIN,
		PRIO_STATE_LAST
	} prioritizationState;

	/** Loads all core modules (core_*)
	 */
	void LoadCoreModules(std::map<std::string, ServiceList>& servicemap);

	/** Calls the Prioritize() method in all loaded modules
	 * @return True if all went well, false if a dependency loop was detected
	 */
	bool PrioritizeHooks();

	/** Unregister all user modes or all channel modes owned by a module
	 * @param mod Module whose modes to unregister
	 * @param modetype MODETYPE_USER to unregister user modes, MODETYPE_CHANNEL to unregister channel modes
	 */
	void UnregisterModes(Module* mod, ModeType modetype);

public:
	typedef std::map<std::string, Module*> ModuleMap;

	/** Event handler hooks.
	 * This needs to be public to be used by FOREACH_MOD and friends.
	 */
	Module::List EventHandlers[I_END];

	/** List of data services keyed by name */
	DataProviderMap DataProviders;

	/** A list of ServiceProviders waiting to be registered.
	 * Non-NULL when constructing a Module, NULL otherwise.
	 * When non-NULL ServiceProviders add themselves to this list on creation and the core
	 * automatically registers them (that is, call AddService()) after the Module is constructed,
	 * and before Module::init() is called.
	 * If a service is created after the construction of the Module (for example in init()) it
	 * has to be registered manually.
	 */
	ServiceList* NewServices;

	/** Expands the name of a module by prepending "m_" and appending ".so".
	 * No-op if the name is already expanded.
	 * @param modname Module name to expand
	 * @return Module name starting with "m_" and ending with ".so"
	 */
	static std::string ExpandModName(const std::string& modname);

	/** Shrinks the name of a module by removing the "m_" prefix and ".so" suffix.
	 * No-op if the name is already shrunk.
	 * @param modname Module name to expand
	 * @return Module name with the "m_" prefix and ".so" suffix removed.
	 */
	static std::string ShrinkModName(const std::string& modname);

	/** Change the priority of one event in a module.
	 * Each module event has a list of modules which are attached to that event type.
	 * If you wish to be called before or after other specific modules, you may use this
	 * method (usually within void Module::Prioritize()) to set your events priority.
	 * You may use this call in other methods too, however, this is not supported behaviour
	 * for a module.
	 * @param mod The module to change the priority of
	 * @param i The event to change the priority of
	 * @param s The state you wish to use for this event. Use one of
	 * PRIO_FIRST to set the event to be first called, PRIO_LAST to
	 * set it to be the last called, or PRIO_BEFORE and PRIORITY_AFTER
	 * to set it to be before or after one or more other modules.
	 * @param which If PRIO_BEFORE or PRIORITY_AFTER is set in parameter 's',
	 * then this contains a the module that your module must be placed before
	 * or after.
	 */
	bool SetPriority(Module* mod, Implementation i, Priority s, Module* which = nullptr);

	/** Change the priority of all events in a module.
	 * @param mod The module to set the priority of
	 * @param s The priority of all events in the module.
	 * Note that with this method, it is not possible to effectively use
	 * PRIO_BEFORE or PRIORITY_AFTER, you should use the more fine tuned
	 * SetPriority method for this, where you may specify other modules to
	 * be prioritized against.
	 */
	void SetPriority(Module* mod, Priority s);

	/** Attach an event to a module.
	 * You may later detach the event with ModuleManager::Detach().
	 * If your module is unloaded, all events are automatically detached.
	 * @param i Event type to attach
	 * @param mod Module to attach event to
	 * @return True if the event was attached
	 */
	bool Attach(Implementation i, Module* mod);

	/** Attach an array of events to a module
	 * @param i Event types (array) to attach
	 * @param mod Module to attach events to
	 * @param sz The size of the implementation array
	 */
	void Attach(const Implementation* i, Module* mod, size_t sz);

	/** Attach all events to a module (used on module load)
	 * @param mod Module to attach to all events
	 */
	void AttachAll(Module* mod);

	/** Detach an event from a module.
	 * This is not required when your module unloads, as the core will
	 * automatically detach your module from all events it is attached to.
	 * @param i Event type to detach
	 * @param mod Module to detach event from
	 * @return True if the event was detached
	 */
	bool Detach(Implementation i, Module* mod);

	/** Detach an array of events from a module
	 * This is not required when your module unloads, as the core will
	 * automatically detach your module from all events it is attached to.
	 * @param i Event types (array) to detach
	 * @param mod Module to detach events from
	 * @param sz The size of the implementation array
	 */
	void Detach(const Implementation* i, Module* mod, size_t sz);

	/** Detach all events from a module (used on unload)
	 * @param mod Module to detach from
	 */
	void DetachAll(Module* mod);

	/** Returns text describing the last module error
	 * @return The last error message to occur
	 */
	std::string& LastError();

	/** Load a given module file
	 * @param filename The file to load
	 * @param defer Defer module init (loading many modules)
	 * @return True if the module was found and loaded
	 */
	bool Load(const std::string& filename, bool defer = false);

	/** Unload a given module file. Note that the module will not be
	 * completely gone until the cull list has finished processing.
	 *
	 * @return true on success; if false, LastError will give a reason
	 */
	bool Unload(Module* module);

	/** Called by the InspIRCd constructor to load all modules from the config file.
	 */
	void LoadAll();
	void UnloadAll();
	void DoSafeUnload(Module*);

	/** Check if a module can be unloaded and if yes, prepare it for unload
	 * @param mod Module to be unloaded
	 * @return True if the module is unloadable, false otherwise.
	 * If true the module must be unloaded in the current main loop iteration.
	 */
	bool CanUnload(Module* mod);

	/** Find a module by name, and return a Module* to it.
	 * This is preferred over iterating the module lists yourself.
	 * @param name The module name to look up
	 * @return A pointer to the module, or NULL if the module cannot be found
	 */
	Module* Find(const std::string& name);

	/** Register a service provided by a module */
	void AddService(ServiceProvider&);

	/** Unregister a service provided by a module */
	void DelService(ServiceProvider&);

	/** Register all services in a given ServiceList
	 * @param list The list containing the services to register
	 */
	void AddServices(const ServiceList& list);

	inline void AddServices(ServiceProvider** list, int count)
	{
		for(int i=0; i < count; i++)
			AddService(*list[i]);
	}

	/** Find a service by name.
	 * If multiple modules provide a given service, the first one loaded will be chosen.
	 */
	ServiceProvider* FindService(ServiceType Type, const std::string& name);

	template<typename T> inline T* FindDataService(const std::string& name)
	{
		return static_cast<T*>(FindService(SERVICE_DATA, name));
	}

	/** Get a map of all loaded modules keyed by their name
	 * @return A ModuleMap containing all loaded modules
	 */
	const ModuleMap& GetModules() const { return Modules; }

	/** Make a service referenceable by dynamic_references
	 * @param name Name that will be used by dynamic_references to find the object
	 * @param service Service to make referenceable by dynamic_references
	 */
	void AddReferent(const std::string& name, ServiceProvider* service);

	/** Make a service no longer referenceable by dynamic_references
	 * @param service Service to make no longer referenceable by dynamic_references
	 */
	void DelReferent(ServiceProvider* service);
};

/** @def FOREACH_MOD(EVENT, ARGS)
 * Run the given hook.
 */
#define FOREACH_MOD(EVENT, ARGS) \
	do \
		{ \
		const Module::List& _handlers = ServerInstance->Modules.EventHandlers[I_ ## EVENT]; \
		for (Module::List::const_reverse_iterator _handler = _handlers.rbegin(); _handler != _handlers.rend(); ) \
		{ \
			Module* _mod = *_handler++; \
			try \
			{ \
				if (!_mod->dying) \
					_mod->EVENT ARGS; \
			} \
			catch (const CoreException& _exception_ ## EVENT) \
			{ \
				ServerInstance->Logs.Debug("MODULE", _mod->ModuleFile + " threw an exception in " # EVENT ": " + (_exception_ ## EVENT).GetReason()); \
			} \
		} \
	} while (false)

/** @def FIRST_MOD_RESULT(EVENT, RESULT, ARGS)
 * Run the given hook until some module returns MOD_RES_ALLOW or MOD_RES_DENY. If no module does
 * that the result is set to MOD_RES_PASSTHRU.
 */
#define FIRST_MOD_RESULT(EVENT, RESULT, ARGS) \
	do \
	{ \
		RESULT = MOD_RES_PASSTHRU; \
		const Module::List& _handlers = ServerInstance->Modules.EventHandlers[I_ ## EVENT]; \
		for (Module::List::const_reverse_iterator _handler = _handlers.rbegin(); _handler != _handlers.rend(); ) \
		{ \
			Module* _mod = *_handler++; \
			try \
			{ \
				if (_mod->dying) \
					continue; \
				RESULT = _mod->EVENT ARGS; \
				if (RESULT != MOD_RES_PASSTHRU) \
					break; \
			} \
			catch (const CoreException& _exception_ ## EVENT) \
			{ \
				ServerInstance->Logs.Debug("MODULE", _mod->ModuleFile + " threw an exception in " # EVENT ": " + (_exception_ ## EVENT).GetReason()); \
			} \
		} \
	} while (false)
