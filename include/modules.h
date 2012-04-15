/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef MODULES_H
#define MODULES_H

#include <string>
#include <deque>
#include <sstream>

/** Used to define a set of behavior bits for a module
 */
enum ModuleFlags {
	VF_NONE = 0,		// module is not special at all
	VF_STATIC = 1,		// module is static, cannot be /unloadmodule'd
	VF_VENDOR = 2,		// module is a vendor module (came in the original tarball, not 3rd party)
	VF_COMMON = 4,		// module needs to be common on all servers in a network to link
	VF_OPTCOMMON = 8,	// module should be common on all servers for unsurprising behavior
	VF_CORE = 16		// module is a core command, can be assumed loaded on all servers
};

/** Used to represent an event type, for user, channel or server
 */
enum TargetTypeFlags {
	TYPE_USER = 1,
	TYPE_CHANNEL,
	TYPE_SERVER,
	TYPE_OTHER
};

#define MOD_RES_ALLOW (ModResult(1))
#define MOD_RES_PASSTHRU (ModResult(0))
#define MOD_RES_DENY (ModResult(-1))

/** Used to represent an allow/deny module result.
 * Not constructed as an enum because it reverses the value logic of some functions;
 * the compiler will inline accesses to have the same efficiency as integer operations.
 */
struct ModResult {
	int res;
	ModResult() : res(0) {}
	explicit ModResult(int r) : res(r) {}
	inline bool operator==(const ModResult& r) const
	{
		return res == r.res;
	}
	inline bool operator!=(const ModResult& r) const
	{
		return res != r.res;
	}
	inline bool operator!() const
	{
		return !res;
	}
	inline bool check(bool def) const
	{
		return (res == 1 || (res == 0 && def));
	}
	/**
	 * Merges two results, preferring ALLOW to DENY
	 */
	inline ModResult operator+(const ModResult& r) const
	{
		if (res == r.res || r.res == 0)
			return *this;
		if (res == 0)
			return r;
		// they are different, and neither is passthru
		return MOD_RES_ALLOW;
	}
};

/** InspIRCd major version.
 * 1.2 -> 102; 2.1 -> 201; 2.12 -> 212
 */
#define INSPIRCD_VERSION_MAJ 201
/** InspIRCd API version.
 * If you change any API elements, increment this value. This counter should be
 * reset whenever the major version is changed. Modules can use these two values
 * and numerical comparisons in preprocessor macros if they wish to support
 * multiple versions of InspIRCd in one file.
 */
#define INSPIRCD_VERSION_API 9

/**
 * This #define allows us to call a method in all
 * loaded modules in a readable simple way, e.g.:
 * 'FOREACH_MOD(I_OnConnect,OnConnect(user));'
 */
#define FOREACH_MOD(y,x) do { \
	EventHandlerIter safei; \
	for (EventHandlerIter _i = ServerInstance->Modules->EventHandlers[y].begin(); _i != ServerInstance->Modules->EventHandlers[y].end(); ) \
	{ \
		CrashState foreach_crash("FOREACH_MOD " #y HERE_STR, *_i); \
		safei = _i; \
		++safei; \
		try \
		{ \
			(*_i)->x ; \
		} \
		catch (CoreException& modexcept) \
		{ \
			ServerInstance->Logs->Log("MODULE",DEFAULT,"Exception caught: %s",modexcept.GetReason()); \
		} \
		_i = safei; \
	} \
} while (0)

#define FOR_EACH_MOD(n,args) do { \
	EventHandlerIter iter_ ## n = ServerInstance->Modules->EventHandlers[I_ ## n].begin(); \
	while (iter_ ## n != ServerInstance->Modules->EventHandlers[I_ ## n].end()) \
	{ \
		Module* mod_ ## n = *iter_ ## n; \
		CrashState fe_crashifo_ ## n("FOR_EACH_MOD " #n HERE_STR, mod_ ## n); \
		iter_ ## n ++; \
		try \
		{ \
			(mod_ ## n)->n args; \
		} \
		catch (CoreException& modexcept) \
		{ \
			ServerInstance->Logs->Log("MODULE",DEFAULT,"Exception caught: %s",modexcept.GetReason()); \
		} \
	} \
} while (0)


/**
 * Custom module result handling loop. This is a paired macro, and should only
 * be used with while_each_hook.
 *
 * See src/channels.cpp for an example of use.
 */
#define DO_EACH_HOOK(n,v,args) \
do { \
	EventHandlerIter iter_ ## n = ServerInstance->Modules->EventHandlers[I_ ## n].begin(); \
	while (iter_ ## n != ServerInstance->Modules->EventHandlers[I_ ## n].end()) \
	{ \
		Module* mod_ ## n = *iter_ ## n; \
		CrashState de_crashifo_ ## n("DO_EACH_HOOK " #n HERE_STR, mod_ ## n); \
		iter_ ## n ++; \
		try \
		{ \
			v = (mod_ ## n)->n args;

#define WHILE_EACH_HOOK(n) \
		} \
		catch (CoreException& except_ ## n) \
		{ \
			ServerInstance->Logs->Log("MODULE",DEFAULT,"Exception caught: %s", (except_ ## n).GetReason()); \
			(void) mod_ ## n; /* catch mismatched pairs */ \
		} \
	} \
} while(0)

/**
 * Module result iterator
 * Runs the given hook until some module returns a useful result.
 *
 * Example: ModResult result;
 * FIRST_MOD_RESULT(OnUserPreNick, result, (user, newnick))
 */
#define FIRST_MOD_RESULT(n,v,args) do { \
	v = MOD_RES_PASSTHRU; \
	DO_EACH_HOOK(n,v,args) \
	{ \
		if (v != MOD_RES_PASSTHRU) \
			break; \
	} \
	WHILE_EACH_HOOK(n); \
} while (0)

/** Holds a module's Version information.
 * The members (set by the constructor only) indicate details as to the version number
 * of a module. A class of type Version is returned by the GetVersion method of the Module class.
 */
class CoreExport Version
{
 public:
	/** Module description
	 */
	const std::string description;

	/** Flags
	 */
	const int Flags;

	/** Server linking description string */
	const std::string link_data;

	/** Simple module version */
	Version(const std::string &desc, int flags = VF_NONE);

	/** Complex version information, including linking compatability data */
	Version(const std::string &desc, int flags, const std::string& linkdata);

	virtual ~Version() {}
};

/** The Event class is a broadcast message directed at all modules.
 * When the class is properly instantiated it may be sent to all modules
 * using the Send() method, which will trigger the OnEvent method in
 * all modules passing the object as its parameter.
 */
class CoreExport Event : public classbase
{
 public:
	/** This is a pointer to the sender of the message, which can be used to
	 * directly trigger events, or to create a reply.
	 */
	ModuleRef source;
	/** The event identifier.
	 * This is arbitary text which should be used to distinguish
	 * one type of event from another.
	 */
	const std::string id;

	/** Create a new Event
	 */
	Event(Module* src, const std::string &eventid);
	/** Send the Event.
	 * The return result of an Event::Send() will always be NULL as
	 * no replies are expected.
	 */
	void Send();
};

class CoreExport DataProvider : public ServiceProvider
{
 public:
	DataProvider(Module* Creator, const std::string& Name)
		: ServiceProvider(Creator, Name, SERVICE_DATA) {}
};

inline User* IS_USER(Extensible* e)
{
	return e && e->type_id == EXTENSIBLE_USER ? static_cast<User*>(e) : NULL;
}

inline Channel* IS_CHANNEL(Extensible* e)
{
	return e && e->type_id == EXTENSIBLE_CHANNEL ? static_cast<Channel*>(e) : NULL;
}

class CoreExport dynamic_reference_base : public interfacebase
{
 private:
	std::string name;
 protected:
	DataProvider* value;
 public:
	dynamic_reference_base(const std::string& Name);
	~dynamic_reference_base();
	inline void ClearCache() { value = NULL; }
	inline const std::string& GetProvider() { return name; }
	void SetProvider(const std::string& newname);
	void lookup();
	operator bool();
	static void reset_all();
};

template<typename T>
class dynamic_reference : public dynamic_reference_base
{
 public:
	dynamic_reference(const std::string& Name)
		: dynamic_reference_base(Name) {}
	inline T* operator->()
	{
		if (!value)
			lookup();
		return static_cast<T*>(value);
	}
	inline operator T*()
	{
		if (!value)
			lookup();
		return static_cast<T*>(value);
	}
};

/** Priority types which can be used by Module::Prioritize()
 */
enum Priority { PRIORITY_FIRST, PRIORITY_LAST, PRIORITY_BEFORE, PRIORITY_AFTER };

/** Implementation-specific flags which may be set in Module::Implements()
 */
enum Implementation
{
	I_ModuleInit,
	I_OnUserConnect, I_OnUserQuit, I_OnUserDisconnect, I_OnUserJoin, I_OnUserPart,
	I_OnSendSnotice, I_OnUserKick, I_OnOper, I_OnInfo, I_OnWhois,
	I_OnUserInvite, I_OnUserPreMessage, I_OnUserPreNotice, I_OnUserPreNick,
	I_OnUserMessage, I_OnUserNotice, I_OnMode, I_OnGetServerDescription, I_OnSyncUser,
	I_OnSyncChannel, I_OnDecodeMetaData, I_OnWallops, I_OnAcceptConnection, I_OnUserInit,
	I_OnChangeHost, I_OnChangeName, I_OnAddLine, I_OnDelLine, I_OnExpireLine,
	I_OnUserPostNick, I_OnPreMode, I_On005Numeric, I_OnKill, I_OnRemoteKill, I_OnLoadModule,
	I_OnUnloadModule, I_OnBackgroundTimer, I_OnPreCommand, I_OnCheckReady,
	I_OnRawMode, I_OnCheckBan, I_OnCheckChannelBan, I_OnExtBanCheck,
	I_OnStats, I_OnPermissionCheck, I_OnCheckJoin,
	I_OnPostTopicChange, I_OnEvent, I_OnPostConnect,
	I_OnUserRegister, I_OnChannelPreDelete, I_OnChannelDelete,
	I_OnPostOper, I_OnSyncNetwork, I_OnSetAway, I_OnPostCommand, I_OnPostJoin,
	I_OnWhoisLine, I_OnBuildNeighborList, I_OnGarbageCollect, I_OnSetConnectClass,
	I_OnText, I_OnPassCompare, I_OnNamesListItem,
	I_OnModuleRehash, I_OnSendWhoLine, I_OnChangeIdent,
	I_END
};

class CoreExport PermissionData : public interfacebase
{
 public:
	/** User that is performing this action */
	User* const source;
	/** Channel that this action is targeted at, or null */
	Channel* const chan;
	/** User that this action is targeted at, or null */
	User* const user;
	/** Name of the permission we would like to check */
	const std::string name;
	/** Whether or not the permission is being implicitly requested
	 * This would be no for most permissions, but would be yes for
	 * ones such as exempt/stripcolor and auditorium/visible.
	 */
	const bool implicit;
	/** Result of the permission check.
	 * MOD_RES_ALLOW will allow the action (skipping any built-in checks)
	 * MOD_RES_DENY will deny the action. If reason is nonempty, this will
	 * be sent to the user as an explanation.
	 * MOD_RES_PASSTHRU will produce the default ircd behavior.
	 */
	ModResult result;
	/** Reason the permission was denied */
	std::string reason;
	PermissionData(User* src, const std::string& Name, Channel* c, User* u, bool i)
		: source(src), chan(c), user(u), name(Name), implicit(i) {}

	/** Convenience formatter class for setting reason as a printf */
	void SetReason(const char* format, ...) CUSTOM_PRINTF(2, 3);
	/** Convenience formatter class for setting reason as a numeric */
	void ErrorNumeric(int id, const char* format, ...) CUSTOM_PRINTF(3, 4);
};

class CoreExport ModePermissionData : public PermissionData
{
 public:
	irc::modechange& mc;
	ModePermissionData(User* src, const std::string& Name, Channel* c, User* u, irc::modechange& m)
		: PermissionData(src, Name, c, u, false), mc(m) {}
	void DoRankCheck();
};

/**
 * Permission data for a channel join. This is first sent to the OnCheckJoin
 * hook. If the result is still MOD_RES_PASSTHRU, normal channel rules are
 * applied (key, limit, invex, ban). Finally, it is passed to OnPermissionCheck
 * and the result is used to determine if the join succeeds.
 */
class CoreExport ChannelPermissionData : public PermissionData
{
 public:
	/** Channel name; is present even when creating the channel (when chan is null) */
	const std::string channel;
	/** Key specified by the user (empty if none) */
	std::string key;
	/** Initial privileges of the Membership (applied prior to OnUserJoin) */
	std::string privs;
	/** True if the user was invited */
	bool invited;
	/** True if the user needs an invite to join */
	bool needs_invite;
	ChannelPermissionData(User* src, Channel* c, const std::string& Name, const std::string& Key)
		: PermissionData(src, "join", c, src, false), channel(Name), key(Key), invited(false), needs_invite(false) {}
};

class CoreExport OperPermissionData : public PermissionData
{
 public:
	/** Oper block being used to oper; NULL if none found */
	reference<OperInfo> oper;
	OperPermissionData(User* who, const std::string& name);
};

/** Base class for all InspIRCd modules
 *  This class is the base class for InspIRCd modules. All modules must inherit from this class,
 *  its methods will be called when irc server events occur. class inherited from module must be
 *  instantiated by the ModuleFactory class (see relevent section) for the module to be initialised.
 */
class CoreExport Module : public classbase, public usecountbase
{
 public:
	/** File that this module was loaded from
	 */
	std::string ModuleSourceFile;
	/** Reference to the dlopen() value
	 */
	DLLManager* ModuleDLLManager;

	/** Default constructor.
	 * Creates a module class. Don't do any type of hook registration or checks
	 * for other modules here; do that in init().
	 */
	Module();

	/** Module setup. Add the hooks you need here; without this, your module won't do anything.
	 * \exception ModuleException Throwing this class, or any class derived from ModuleException, causes loading of the module to abort.
	 */
	virtual void early_init();

	/** Configuration reading hook. Called after early_init() and before init() on module load, and on a rehash.
	 */
	virtual void ReadConfig(ConfigReadStatus&);

	/** Module setup. Add the hooks you need here; without this, your module won't do anything.
	 * \exception ModuleException Throwing this class, or any class derived from ModuleException, causes loading of the module to abort.
	 */
	virtual void init() = 0;

	/** Clean up prior to destruction
	 * If you override, you must call this AFTER your module's cleanup
	 */
	virtual CullResult cull();

	/** Default destructor.
	 * destroys a module class
	 */
	virtual ~Module();

	virtual void Prioritize()
	{
	}

	/** Returns the version number of a Module.
	 * The method should return a Version object with its version information assigned via
	 * Version::Version
	 */
	virtual Version GetVersion() = 0;
	
	//////////////////////////////////////////////////////////////////////////////////////
	// All functions below here are never called unless you register for them in init() //
	//////////////////////////////////////////////////////////////////////////////////////

	/** Called when a user connects.
	 * The details of the connecting user are available to you in the parameter User *user
	 * @param user The user who is connecting
	 */
	virtual void OnUserConnect(LocalUser* user);

	/** Called when a user quits.
	 * The details of the exiting user are available to you in the parameter User *user
	 * This event is only called when the user is fully registered when they quit. To catch
	 * raw disconnections, use the OnUserDisconnect method.
	 * @param user The user who is quitting
	 * @param message The user's quit message (as seen by non-opers)
	 * @param oper_message The user's quit message (as seen by opers)
	 */
	virtual void OnUserQuit(User* user, const std::string &message, const std::string &oper_message);

	/** Called whenever a user's socket is closed.
	 * The details of the exiting user are available to you in the parameter User *user
	 * This event is called for all users, registered or not, as a cleanup method for modules
	 * which might assign resources to user, such as dns lookups, objects and sockets.
	 * @param user The user who is disconnecting
	 */
	virtual void OnUserDisconnect(LocalUser* user);

	/** Called whenever a channel is about to be deleted
	 * @param chan The channel being deleted
	 * @return An integer specifying whether or not the channel may be deleted. 0 for yes, 1 for no.
	 */
	virtual ModResult OnChannelPreDelete(Channel *chan);

	/** Called whenever a channel is deleted, either by QUIT, KICK or PART.
	 * @param chan The channel being deleted
	 */
	virtual void OnChannelDelete(Channel* chan);

	/** Called when a user joins a channel.
	 * The details of the joining user are available to you in the parameter User *user,
	 * and the details of the channel they have joined is available in the variable Channel *channel
	 * @param memb The channel membership being created
	 * @param sync This is set to true if the JOIN is the result of a network sync and the remote user is being introduced
	 * to a channel due to the network sync.
	 * @param created This is true if the join created the channel
	 */
	virtual void OnUserJoin(Membership* memb, bool sync, bool created, CUList& except_list);

	/** Called after a user joins a channel
	 * Identical to OnUserJoin, but called immediately afterwards, when any linking module has
	 * seen the join. Note this is also called when a channel's timestamp is being lowered.
	 *
	 * @param memb The channel membership created
	 */
	virtual void OnPostJoin(Membership*);

	/** Called when a user parts a channel.
	 * The details of the leaving user are available to you in the parameter User *user,
	 * and the details of the channel they have left is available in the variable Channel *channel
	 * @param memb The channel membership being destroyed
	 * @param partmessage The part message, or an empty string (may be modified)
	 */
	virtual void OnUserPart(Membership* memb, std::string &partmessage, CUList& except_list);

	/** Called on rehash.
	 * This method is called when a user initiates a module-specific rehash. This can be used to do
	 * expensive operations (such as reloading SSL certificates) that are not executed on a normal
	 * rehash for efficiency. A rehash of this type does not reload the core configuration.
	 *
	 * @param user The user performing the rehash.
	 * @param parameter The parameter given to REHASH
	 */
	virtual void OnModuleRehash(User* user, const std::string &parameter);

	/** Called whenever a snotice is about to be sent to a snomask.
	 * snomask and type may both be modified; the message may not.
	 * @param snomask The snomask the message is going to (e.g. 'A')
	 * @param type The textual description the snomask will go to (e.g. 'OPER')
	 * @param message The text message to be sent via snotice
	 * @return 1 to block the snotice from being sent entirely, 0 else.
	 */
	virtual ModResult OnSendSnotice(char &snomask, std::string &type, const std::string &message);

	/** Called whenever a user is about to join a channel. This hook is
	 * called prior to the checks for channel ban, limit, invite, etc; the
	 * normal OnPermissionCheck hook is called after those checks.
	 *
	 * Change the result of the join to MOD_RES_DENY to forbid the join, or
	 * to MOD_RES_ALLOW to skip further checks.
	 *
	 * @param join A description of the join to check
	 */
	virtual void OnCheckJoin(ChannelPermissionData& join);

	/** Called whenever a user is kicked.
	 * @param source The user issuing the kick
	 * @param user The user being kicked
	 * @param chan The channel the user is being kicked from
	 * @param reason The kick reason
	 */
	virtual void OnUserKick(User* source, Membership* memb, const std::string &reason, CUList& except_list);

	/** Called whenever a user opers locally.
	 * The User will contain the oper mode 'o' as this function is called after any modifications
	 * are made to the user's structure by the core.
	 * @param user The user who is opering up
	 * @param opertype The opers type name
	 */
	virtual void OnOper(User* user, const std::string &opertype);

	/** Called after a user opers locally.
	 * This is identical to Module::OnOper(), except it is called after OnOper so that other modules
	 * can be gauranteed to already have processed the oper-up, for example m_spanningtree has sent
	 * out the OPERTYPE, etc.
	 * @param user The user who is opering up
	 * @param opername The name of the oper that the user is opering up to. Only valid locally. Empty string otherwise.
	 * @param opertype The opers type name
	 */
	virtual void OnPostOper(User* user, const std::string &opername, const std::string &opertype);

	/** Called whenever a user types /INFO.
	 * The User will contain the information of the user who typed the command. Modules may use this
	 * method to output their own credits in /INFO (which is the ircd's version of an about box).
	 * It is purposefully not possible to modify any info that has already been output, or halt the list.
	 * You must write a 371 numeric to the user, containing your info in the following format:
	 *
	 * &lt;nick&gt; :information here
	 *
	 * @param user The user issuing /INFO
	 */
	virtual void OnInfo(User* user);

	/** Called whenever a /WHOIS is performed on a local user.
	 * The source parameter contains the details of the user who issued the WHOIS command, and
	 * the dest parameter contains the information of the user they are whoising.
	 * @param source The user issuing the WHOIS command
	 * @param dest The user who is being WHOISed
	 */
	virtual void OnWhois(User* source, User* dest);

	/** Called after a user has been successfully invited to a channel.
	 * @param source The user who is issuing the INVITE
	 * @param dest The user being invited
	 * @param channel The channel the user is being invited to
	 * @param timeout The time the invite will expire (0 == never)
	 */
	virtual void OnUserInvite(User* source,User* dest,Channel* channel, time_t timeout);

	/** Called whenever a user is about to PRIVMSG A user or a channel, before any processing is done.
	 * Returning any nonzero value from this function stops the process immediately, causing no
	 * output to be sent to the user by the core. If you do this you must produce your own numerics,
	 * notices etc. This is useful for modules which may want to filter or redirect messages.
	 * target_type can be one of TYPE_USER or TYPE_CHANNEL. If the target_type value is a user,
	 * you must cast dest to a User* otherwise you must cast it to a Channel*, this is the details
	 * of where the message is destined to be sent.
	 * @param user The user sending the message
	 * @param dest The target of the message (Channel* or User*)
	 * @param target_type The type of target (TYPE_USER or TYPE_CHANNEL)
	 * @param text Changeable text being sent by the user
	 * @param status The status being used, e.g. PRIVMSG @#chan has status== '@', 0 to send to everyone.
	 * @param exempt_list A list of users not to send to. For channel messages, this will usually contain just the sender.
	 * It will be ignored for private messages.
	 * @return 1 to deny the message, 0 to allow it
	 */
	virtual ModResult OnUserPreMessage(User* user,void* dest,int target_type, std::string &text,char status, CUList &exempt_list);

	/** Called whenever a user is about to NOTICE A user or a channel, before any processing is done.
	 * Returning any nonzero value from this function stops the process immediately, causing no
	 * output to be sent to the user by the core. If you do this you must produce your own numerics,
	 * notices etc. This is useful for modules which may want to filter or redirect messages.
	 * target_type can be one of TYPE_USER or TYPE_CHANNEL. If the target_type value is a user,
	 * you must cast dest to a User* otherwise you must cast it to a Channel*, this is the details
	 * of where the message is destined to be sent.
	 * You may alter the message text as you wish before relinquishing control to the next module
	 * in the chain, and if no other modules block the text this altered form of the text will be sent out
	 * to the user and possibly to other servers.
	 * @param user The user sending the message
	 * @param dest The target of the message (Channel* or User*)
	 * @param target_type The type of target (TYPE_USER or TYPE_CHANNEL)
	 * @param text Changeable text being sent by the user
	 * @param status The status being used, e.g. PRIVMSG @#chan has status== '@', 0 to send to everyone.
	 * @param exempt_list A list of users not to send to. For channel notices, this will usually contain just the sender.
	 * It will be ignored for private notices.
	 * @return 1 to deny the NOTICE, 0 to allow it
	 */
	virtual ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text,char status, CUList &exempt_list);

	/** Called when sending a message to all "neighbors" of a given user -
	 * that is, all users that share a common channel. This is used in
	 * commands such as NICK, QUIT, etc.
	 * @param source The source of the message
	 * @param include_c Channels to scan for users to include
	 * @param exceptions Map of user->bool that overrides the inclusion decision
	 *
	 * Set exceptions[user] = true to include, exceptions[user] = false to exclude
	 */
	virtual void OnBuildNeighborList(User* source, std::vector<Channel*> &include_c, std::map<User*,bool> &exceptions);

	/** Called before any nickchange, local or remote. This can be used to implement Q-lines etc.
	 * Please note that although you can see remote nickchanges through this function, you should
	 * NOT make any changes to the User if the user is a remote user as this may cause a desnyc.
	 * check user->server before taking any action (including returning nonzero from the method).
	 * If your method returns nonzero, the nickchange is silently forbidden, and it is down to your
	 * module to generate some meaninful output.
	 * @param user The username changing their nick
	 * @param newnick Their new nickname
	 * @return 1 to deny the change, 0 to allow
	 */
	virtual ModResult OnUserPreNick(User* user, const std::string &newnick);

	/** Called after any PRIVMSG sent from a user.
	 * The dest variable contains a User* if target_type is TYPE_USER and a Channel*
	 * if target_type is TYPE_CHANNEL.
	 * @param user The user sending the message
	 * @param dest The target of the message
	 * @param target_type The type of target (TYPE_USER or TYPE_CHANNEL)
	 * @param text the text being sent by the user
	 * @param status The status being used, e.g. PRIVMSG @#chan has status== '@', 0 to send to everyone.
	 */
	virtual void OnUserMessage(User* user, void* dest, int target_type, const std::string &text, char status, const CUList &exempt_list);

	/** Called after any NOTICE sent from a user.
	 * The dest variable contains a User* if target_type is TYPE_USER and a Channel*
	 * if target_type is TYPE_CHANNEL.
	 * @param user The user sending the message
	 * @param dest The target of the message
	 * @param target_type The type of target (TYPE_USER or TYPE_CHANNEL)
	 * @param text the text being sent by the user
	 * @param status The status being used, e.g. NOTICE @#chan has status== '@', 0 to send to everyone.
	 */
	virtual void OnUserNotice(User* user, void* dest, int target_type, const std::string &text, char status, const CUList &exempt_list);

	/** Called immediately before any NOTICE or PRIVMSG sent from a user, local or remote.
	 * The dest variable contains a User* if target_type is TYPE_USER and a Channel*
	 * if target_type is TYPE_CHANNEL.
	 * The difference between this event and OnUserPreNotice/OnUserPreMessage is that delivery is gauranteed,
	 * the message has already been vetted. In the case of the other two methods, a later module may stop your
	 * message. This also differs from OnUserMessage which occurs AFTER the message has been sent.
	 * @param user The user sending the message
	 * @param dest The target of the message
	 * @param target_type The type of target (TYPE_USER or TYPE_CHANNEL)
	 * @param text the text being sent by the user
	 * @param status The status being used, e.g. NOTICE @#chan has status== '@', 0 to send to everyone.
	 */
	virtual void OnText(User* user, void* dest, int target_type, const std::string &text, char status, CUList &exempt_list);

	/** Called after every MODE command sent from a user
	 * @param user The user sending the MODEs
	 * @param dest The target of the modes (User* or Channel*)
	 * @param modes The actual mode changes
	 */
	virtual void OnMode(User* user, Extensible* target, const irc::modestacker& modes);

	/** Allows modules to alter or create server descriptions
	 * Whenever a module requires a server description, for example for display in
	 * WHOIS, this function is called in all modules. You may change or define the
	 * description given in std::string &description. If you do, this description
	 * will be shown in the WHOIS fields.
	 * @param servername The servername being searched for
	 * @param description Alterable server description for this server
	 */
	virtual void OnGetServerDescription(const std::string &servername,std::string &description);

	/** Allows modules to synchronize data which relates to users during a netburst.
	 * When this function is called, it will be called from the module which implements
	 * the linking protocol. This currently is m_spanningtree.so. A pointer to this module
	 * is given in Module* proto, so that you may call its methods such as ProtoSendMode
	 * (see below). This function will be called for every user visible on your side
	 * of the burst, allowing you to for example set modes, etc. Do not use this call to
	 * synchronize data which you have stored using class Extensible -- There is a specialist
	 * function OnSyncUserMetaData and OnSyncChannelMetaData for this!
	 * @param user The user being syncronized
	 * @param opaque An object that is used to send the sync data
	 */
	virtual void OnSyncUser(User* user, SyncTarget* opaque);

	/** Allows modules to synchronize data which relates to channels during a netburst.
	 * When this function is called, it will be called from the module which implements
	 * the linking protocol. This currently is m_spanningtree.so. A pointer to this module
	 * is given in Module* proto, so that you may call its methods such as ProtoSendMode
	 * (see below). This function will be called for every user visible on your side
	 * of the burst, allowing you to for example set modes, etc.
	 *
	 * For a good example of how to use this function, please see src/modules/m_chanprotect.cpp
	 *
	 * @param chan The channel being syncronized
	 * @param proto A pointer to the module handling network protocol
	 * @param opaque An object that is used to send the sync data
	 */
	virtual void OnSyncChannel(Channel* chan, SyncTarget* opaque);

	/* Allows modules to syncronize metadata not related to users or channels, over the network during a netburst.
	 * Whenever the linking module wants to send out data, but doesnt know what the data
	 * represents (e.g. it is Extensible metadata, added to a User or Channel by a module) then
	 * this method is called. You should use the ProtoSendMetaData function after you've
	 * correctly decided how the data should be represented, to send the metadata on its way if
	 * if it belongs to your module.
	 * @param proto A pointer to the module handling network protocol
	 * @param opaque An object that is used to send the sync data
	 */
	virtual void OnSyncNetwork(SyncTarget* opaque);

	/** Allows module data, sent via ProtoSendMetaData, to be decoded again by a receiving module.
	 * Please see src/modules/m_swhois.cpp for a working example of how to use this method call.
	 * @param target_type The type of item to decode data for, TYPE_USER or TYPE_CHANNEL
	 * @param target The Channel* or User* that data should be added to
	 * @param extname The extension name which is being sent
	 * @param extdata The extension data, encoded at the other end by an identical module through OnSyncChannelMetaData or OnSyncUserMetaData
	 */
	virtual void OnDecodeMetaData(Extensible* target, const std::string &extname, const std::string &extdata);

	/**
	 * Primary permission check hook
	 *
	 * Every time the IRCd either denies permission or prevents an action in
	 * a channel due to insufficient access (as opposed to invalid syntax),
	 * this hook should first be called to see if a channel has overridden
	 * the access control.
	 */
	virtual void OnPermissionCheck(PermissionData& permission);

	/** Called after every WALLOPS command.
	 * @param user The user sending the WALLOPS
	 * @param text The content of the WALLOPS message
	 */
	virtual void OnWallops(User* user, const std::string &text);

	/** Called whenever a user's hostname is changed.
	 * This event triggers after the host has been set.
	 * @param user The user whos host is being changed
	 * @param newhost The new hostname being set
	 */
	virtual void OnChangeHost(User* user, const std::string &newhost);

	/** Called whenever a user's GECOS (realname) is changed.
	 * This event triggers after the name has been set.
	 * @param user The user who's GECOS is being changed
	 * @param gecos The new GECOS being set on the user
	 */
	virtual void OnChangeName(User* user, const std::string &gecos);

	/** Called whenever a user's IDENT is changed.
	 * This event triggers after the name has been set.
	 * @param user The user who's IDENT is being changed
	 * @param gecos The new IDENT being set on the user
	 */
	virtual void OnChangeIdent(User* user, const std::string &ident);

	/** Called whenever an xline is added by a local user.
	 * This method is triggered after the line is added.
	 * @param source The sender of the line or NULL for local server
	 * @param line The xline being added
	 */
	virtual void OnAddLine(User* source, XLine* line);

	/** Called whenever an xline is deleted MANUALLY.
	 * This method is triggered after the line is deleted.
	 * @param source The user removing the line or NULL for local server
	 * @param line the line being deleted
	 */
	virtual void OnDelLine(User* source, XLine* line);

	/** Called whenever an xline expires.
	 * This method is triggered after the line is deleted.
	 * @param line The line being deleted.
	 */
	virtual void OnExpireLine(XLine *line);

	/** Called before your module is unloaded to clean up Extensibles.
	 * This method is called once for every user and channel on the network,
	 * so that when your module unloads it may clear up any remaining data
	 * in the form of Extensibles added using Extensible::Extend().
	 * If the target_type variable is TYPE_USER, then void* item refers to
	 * a User*, otherwise it refers to a Channel*.
	 * @param target_type The type of item being cleaned
	 * @param item A pointer to the item's class
	 */
	virtual void OnCleanup(int target_type, void* item);

	/** Called after any nickchange, local or remote. This can be used to track users after nickchanges
	 * have been applied. Please note that although you can see remote nickchanges through this function, you should
	 * NOT make any changes to the User if the user is a remote user as this may cause a desnyc.
	 * check user->server before taking any action (including returning nonzero from the method).
	 * Because this method is called after the nickchange is taken place, no return values are possible
	 * to indicate forbidding of the nick change. Use OnUserPreNick for this.
	 * @param user The user changing their nick
	 * @param oldnick The old nickname of the user before the nickchange
	 */
	virtual void OnUserPostNick(User* user, const std::string &oldnick);

	/** Called before any mode change, to allow a single access check for
	 * a full mode change (use OnRawMode to check individual modes)
	 *
	 * Returning MOD_RES_ALLOW will skip prefix level checks, but can be overridden by
	 * OnRawMode for each individual mode. Returning MOD_RES_DENY will deny all changes.
	 *
	 * @param source the user making the mode change
	 * @param target the user or channel having its modes changed
	 * @param modes the mode changes being made
	 */
	virtual ModResult OnPreMode(User* source, Extensible* target, irc::modestacker& modes);

	/** Called when a 005 numeric is about to be output.
	 * The module should modify the 005 numeric if needed to indicate its features.
	 * @param output The 005 string to be modified if neccessary.
	 */
	virtual void On005Numeric(std::string &output);

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
	virtual ModResult OnKill(User* source, User* dest, const std::string &reason);

	/** Called when an oper wants to disconnect a remote user via KILL
	 * @param source The user sending the KILL
	 * @param dest The user being killed
	 * @param reason The kill reason
	 */
	virtual void OnRemoteKill(User* source, User* dest, const std::string &reason, const std::string &operreason);

	/** Called whenever a module is loaded.
	 * mod will contain a pointer to the module, and string will contain its name,
	 * for example m_widgets.so. This function is primary for dependency checking,
	 * your module may decide to enable some extra features if it sees that you have
	 * for example loaded "m_killwidgets.so" with "m_makewidgets.so". It is highly
	 * recommended that modules do *NOT* bail if they cannot satisfy dependencies,
	 * but instead operate under reduced functionality, unless the dependency is
	 * absolutely neccessary (e.g. a module that extends the features of another
	 * module).
	 * @param mod A pointer to the new module
	 */
	virtual void OnLoadModule(Module* mod);

	/** Called whenever a module is unloaded.
	 * mod will contain a pointer to the module, and string will contain its name,
	 * for example m_widgets.so. This function is primary for dependency checking,
	 * your module may decide to enable some extra features if it sees that you have
	 * for example loaded "m_killwidgets.so" with "m_makewidgets.so". It is highly
	 * recommended that modules do *NOT* bail if they cannot satisfy dependencies,
	 * but instead operate under reduced functionality, unless the dependency is
	 * absolutely neccessary (e.g. a module that extends the features of another
	 * module).
	 * @param mod Pointer to the module being unloaded (still valid)
	 * @param name The filename of the module being unloaded
	 */
	virtual void OnUnloadModule(Module* mod);

	/** Called once every five seconds for background processing.
	 * This timer can be used to control timed features. Its period is not accurate
	 * enough to be used as a clock, but it is gauranteed to be called at least once in
	 * any five second period, directly from the main loop of the server.
	 * @param curtime The current timer derived from time(2)
	 */
	virtual void OnBackgroundTimer(time_t curtime);

	/** Called whenever any command is about to be executed.
	 * This event occurs for all registered commands, wether they are registered in the core,
	 * or another module, and for invalid commands. Invalid commands may only be sent to this
	 * function when the value of validated is false. By returning 1 from this method you may prevent the
	 * command being executed. If you do this, no output is created by the core, and it is
	 * down to your module to produce any output neccessary.
	 * Note that unless you return 1, you should not destroy any structures (e.g. by using
	 * InspIRCd::QuitUser) otherwise when the command's handler function executes after your
	 * method returns, it will be passed an invalid pointer to the user object and crash!)
	 * @param command The command being executed
	 * @param parameters An array of array of characters containing the parameters for the command
	 * @param pcnt The nuimber of parameters passed to the command
	 * @param user the user issuing the command
	 * @param validated True if the command has passed all checks, e.g. it is recognised, has enough parameters, the user has permission to execute it, etc.
	 * You should only change the parameter list and command string if validated == false (e.g. before the command lookup occurs).
	 * @param original_line The entire original line as passed to the parser from the user
	 * @return 1 to block the command, 0 to allow
	 */
	virtual ModResult OnPreCommand(std::string &command, std::vector<std::string>& parameters, LocalUser *user, bool validated, const std::string &original_line);

	/** Called after any command has been executed.
	 * This event occurs for all registered commands, wether they are registered in the core,
	 * or another module, but it will not occur for invalid commands (e.g. ones which do not
	 * exist within the command table). The result code returned by the command handler is
	 * provided.
	 * @param command The command being executed
	 * @param parameters An array of array of characters containing the parameters for the command
	 * @param pcnt The nuimber of parameters passed to the command
	 * @param user the user issuing the command
	 * @param result The return code given by the command handler, one of CMD_SUCCESS or CMD_FAILURE
	 * @param original_line The entire original line as passed to the parser from the user
	 */
	virtual void OnPostCommand(const std::string &command, const std::vector<std::string>& parameters, LocalUser *user, CmdResult result, const std::string &original_line);

	/** Called when a user is first connecting, prior to starting DNS lookups, checking initial
	 * connect class, or accepting any commands.
	 */
	virtual void OnUserInit(LocalUser* user);

	/** Called to check if a user who is connecting can now be allowed to register
	 * If any modules return false for this function, the user is held in the waiting
	 * state until all modules return true. For example a module which implements ident
	 * lookups will continue to return false for a user until their ident lookup is completed.
	 * Note that the registration timeout for a user overrides these checks, if the registration
	 * timeout is reached, the user is disconnected even if modules report that the user is
	 * not ready to connect.
	 * @param user The user to check
	 * @return true to indicate readiness, false if otherwise
	 */
	virtual ModResult OnCheckReady(LocalUser* user);

	/** Called whenever a user is about to register their connection (e.g. before the user
	 * is sent the MOTD etc). Modules can use this method if they are performing a function
	 * which must be done before the actual connection is completed (e.g. ident lookups,
	 * dnsbl lookups, etc).
	 * Note that you should NOT delete the user record here by causing a disconnection!
	 * Use OnUserConnect for that instead.
	 * @param user The user registering
	 */
	virtual void OnUserRegister(LocalUser* user);

	/** Called whenever a mode character is processed.
	 * Return 1 from this function to block the mode character from being processed entirely.
	 * @param user The user who is sending the mode
	 * @param chan The channel the mode is being sent to (or NULL if a usermode)
	 * @param mode The mode character being set
	 * @param param The parameter for the mode or an empty string
	 * @param adding true of the mode is being added, false if it is being removed
	 * @param pcnt The parameter count for the mode (0 or 1)
	 * @return MOD_RES_DENY to deny the mode, MOD_RES_PASSTHRU to do standard mode checking, and
	 * MOD_RES_ALLOW to skip all permission checking. Please note that for remote mode changes, your
	 * return value will be ignored!
	 */
	virtual ModResult OnRawMode(User* user, Channel* chan, irc::modechange& mc);

	/**
	 * Checks for a user's ban from the channel
	 * @param user The user to check
	 * @param chan The channel to check in
	 * @return MOD_RES_DENY to mark as banned, MOD_RES_ALLOW to skip the
	 * ban check, or MOD_RES_PASSTHRU to check bans normally
	 */
	virtual ModResult OnCheckChannelBan(User* user, Channel* chan);

	/**
	 * Checks for a user's match of a single ban
	 * @param user The user to check for match
	 * @param chan The channel on which the match is being checked
	 * @param mask The mask being checked
	 * @return MOD_RES_DENY to mark as banned, MOD_RES_ALLOW to skip the
	 * ban check, or MOD_RES_PASSTHRU to check bans normally
	 */
	virtual ModResult OnCheckBan(User* user, Channel* chan, const std::string& mask);

	/** Checks for a match on a given extban type
	 * @return MOD_RES_DENY to mark as banned, MOD_RES_ALLOW to skip the
	 * ban check, or MOD_RES_PASSTHRU to check bans normally
	 */
	virtual ModResult OnExtBanCheck(User* user, Channel* chan, char type);

	/** Called on all /STATS commands
	 * This method is triggered for all /STATS use, including stats symbols handled by the core.
	 * @param symbol the symbol provided to /STATS
	 * @param user the user issuing the /STATS command
	 * @param results A string_list to append results into. You should put all your results
	 * into this string_list, rather than displaying them directly, so that your handler will
	 * work when remote STATS queries are received.
	 * @return 1 to block the /STATS from being processed by the core, 0 to allow it
	 */
	virtual ModResult OnStats(char symbol, User* user, string_list &results);

	/** Called whenever a topic has been changed.
	 * @param user The user changing the topic
	 * @param chan The channels who's topic is being changed
	 * @param topic The actual topic text
	 */
	virtual void OnPostTopicChange(User* user, Channel* chan, const std::string &topic);

	/** Called whenever an Event class is sent to all modules by another module.
	 * You should *always* check the value of Event::id to determine the event type.
	 * @param event The Event class being received
	 */
	virtual void OnEvent(Event& event);

	/** Called whenever a password check is to be made. Replaces the old OldOperCompare API.
	 * The password field (from the config file) is in 'password' and is to be compared against
	 * 'input'. This method allows for encryption of passwords (oper, connect:allow, die/restart, etc).
	 * You should return a nonzero value to override the normal comparison, or zero to pass it on.
	 * @param ex The object that's causing the authentication (User* for <oper> <connect:allow> etc, Server* for <link>).
	 * @param password The password from the configuration file (the password="" value).
	 * @param input The password entered by the user or whoever.
	 * @param hashtype The hash value from the config
	 * @return 0 to do nothing (pass on to next module/default), 1 == password is OK, -1 == password is not OK
	 */
	virtual ModResult OnPassCompare(Extensible* ex, const std::string &password, const std::string &input, const std::string& hashtype);

	/** Called after a user has fully connected and all modules have executed OnUserConnect
	 * This event is informational only. You should not change any user information in this
	 * event. To do so, use the OnUserConnect method to change the state of local users.
	 * This is called for both local and remote users.
	 * @param user The user who is connecting
	 */
	virtual void OnPostConnect(User* user);

	/** Called when a port accepts a connection
	 * Return MOD_RES_ACCEPT if you have used the file descriptor.
	 * @param fd The file descriptor returned from accept()
	 * @param from The local port the user connected to
	 * @param client The client IP address and port
	 * @param server The server IP address and port
	 */
	virtual StreamSocket* OnAcceptConnection(int fd, ListenSocket* from, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server);

	/** Called whenever a user sets away or returns from being away.
	 * The away message is available as a parameter, but should not be modified.
	 * At this stage, it has already been copied into the user record.
	 * If awaymsg is empty, the user is returning from away.
	 * @param user The user setting away
	 * @param awaymsg The away message of the user, or empty if returning from away
	 * @return nonzero if the away message should be blocked - should ONLY be nonzero for LOCAL users (IS_LOCAL) (no output is returned by core)
	 */
	virtual ModResult OnSetAway(User* user, const std::string &awaymsg);

	/** Called whenever a line of WHOIS output is sent to a user.
	 * You may change the numeric and the text of the output by changing
	 * the values numeric and text, but you cannot change the user the
	 * numeric is sent to. You may however change the user's User values.
	 * @param user The user the numeric is being sent to
	 * @param dest The user being WHOISed
	 * @param numeric The numeric of the line being sent
	 * @param text The text of the numeric, including any parameters
	 * @return nonzero to drop the line completely so that the user does not
	 * receive it, or zero to allow the line to be sent.
	 */
	virtual ModResult OnWhoisLine(User* user, User* dest, int &numeric, std::string &text);

	/** Called at intervals for modules to garbage-collect any hashes etc.
	 * Certain data types such as hash_map 'leak' buckets, which must be
	 * tidied up and freed by copying into a new item every so often. This
	 * method is called when it is time to do that.
	 */
	virtual void OnGarbageCollect();

	/** Called when a user's connect class is being matched
	 * @return MOD_RES_ALLOW to force the class to match, MOD_RES_DENY to forbid it, or
	 * MOD_RES_PASSTHRU to allow normal matching (by host/port).
	 */
	virtual ModResult OnSetConnectClass(LocalUser* user, ConnectClass* myclass);

	/** Called for every item in a NAMES list, so that modules may reformat portions of it as they see fit.
	 * For example NAMESX, channel mode +u and +I, and UHNAMES. If the nick is set to an empty string by any
	 * module, then this will cause the nickname not to be displayed at all.
	 */
	virtual void OnNamesListItem(User* issuer, Membership* item, std::string &prefixes, std::string &nick);

	/** Called whenever a result from /WHO is about to be returned
	 * @param source The user running the /WHO query
	 * @param params The parameters to the /WHO query
	 * @param user The user that this line of the query is about
	 * @param line The raw line to send; modifiable, if empty no line will be returned.
	 */
	virtual void OnSendWhoLine(User* source, const std::vector<std::string>& params, User* user, std::string& line);

	/** Add test suite hooks here. These are used for testing functionality of a module
	 * via the --testsuite debugging parameter.
	 */
	virtual void RunTestSuite();
};

struct RestoreData {
	/** item to restore to (uuid, channel) */
	std::string item;
	/** name of thing we restore (mode, metadata key) */
	std::string name;
	/** value to restore */
	std::string value;
	RestoreData(const std::string& i, const std::string& k, const std::string& v)
		: item(i), name(k), value(v) {}
};

/** Module state stored between module loads */
class CoreExport ModuleState
{
 public:
	std::vector<RestoreData> channelModes;
	std::vector<RestoreData> channelExt;
	std::vector<RestoreData> userModes;
	std::vector<RestoreData> userExt;
};

/** Caches a text file into memory and can be used to retrieve lines from it.
 * This class contains methods for read-only manipulation of a text file in memory.
 * Either use the constructor type with one parameter to load a file into memory
 * at construction, or use the LoadFile method to load a file.
 */
class CoreExport FileReader : public classbase
{
	/** The file contents
	 */
	std::vector<std::string> fc;

	/** Content size in bytes
	 */
	unsigned long contentsize;

	/** Calculate content size in bytes
	 */
	void CalcSize();

 public:
	/** Default constructor.
	 * This method does not load any file into memory, you must use the LoadFile method
	 * after constructing the class this way.
	 */
	FileReader();

	/** Secondary constructor.
	 * This method initialises the class with a file loaded into it ready for GetLine and
	 * and other methods to be called. If the file could not be loaded, FileReader::FileSize
	 * returns 0.
	 */
	FileReader(const std::string &filename);

	/** Default destructor.
	 * This deletes the memory allocated to the file.
	 */
	~FileReader();

	/** Used to load a file.
	 * This method loads a file into the class ready for GetLine and
	 * and other methods to be called. If the file could not be loaded, FileReader::FileSize
	 * returns 0.
	 */
	void LoadFile(const std::string &filename);

	/** Returns the whole content of the file as std::string
	 */
	std::string Contents();

	/** Returns the entire size of the file as std::string
	 */
	unsigned long ContentSize();

	/** Returns true if the file exists
	 * This function will return false if the file could not be opened.
	 */
	bool Exists();

	/** Retrieve one line from the file.
	 * This method retrieves one line from the text file. If an empty non-NULL string is returned,
	 * the index was out of bounds, or the line had no data on it.
	 */
	std::string GetLine(int x);

	/** Returns the size of the file in lines.
	 * This method returns the number of lines in the read file. If it is 0, no lines have been
	 * read into memory, either because the file is empty or it does not exist, or cannot be
	 * opened due to permission problems.
	 */
	int FileSize();
};

/** A list of modules
 */
typedef std::vector<Module*> IntModuleList;

/** An event handler iterator
 */
typedef IntModuleList::iterator EventHandlerIter;

/** ModuleManager takes care of all things module-related
 * in the core.
 */
class CoreExport ModuleManager
{
 private:
	/** Holds a string describing the last module error to occur
	 */
	std::string LastModuleError;

	/** Total number of modules loaded into the ircd
	 */
	int ModCount;

	/** List of loaded modules and shared object/dll handles
	 * keyed by module name
	 */
	std::map<std::string, Module*> Modules;

	enum {
		PRIO_STATE_FIRST,
		PRIO_STATE_AGAIN,
		PRIO_STATE_LAST
	} prioritizationState;

	/** Internal unload module hook */
	bool CanUnload(Module*);

 public:

	/** Event handler hooks.
	 * This needs to be public to be used by FOREACH_MOD and friends.
	 */
	IntModuleList EventHandlers[I_END];

	/** List of data services keyed by name */
	std::multimap<std::string, ServiceProvider*> DataProviders;

	/** Simple, bog-standard, boring constructor.
	 */
	ModuleManager();

	/** Destructor
	 */
	~ModuleManager();

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
	bool SetPriority(Module* mod, Implementation i, Priority s, Module* which = NULL);

	/** Backwards compat interface */
	inline bool SetPriority(Module* mod, Implementation i, Priority s, Module** dptr)
	{
		return SetPriority(mod, i, s, *dptr);
	}

	/** Change the priority of all events in a module.
	 * @param mod The module to set the priority of
	 * @param s The priority of all events in the module.
	 * Note that with this method, it is not possible to effectively use
	 * PRIO_BEFORE or PRIORITY_AFTER, you should use the more fine tuned
	 * SetPriority method for this, where you may specify other modules to
	 * be prioritized against.
	 */
	bool SetPriority(Module* mod, Priority s);

	/** Attach an event to a module.
	 * You may later detatch the event with ModuleManager::Detach().
	 * If your module is unloaded, all events are automatically detatched.
	 * @param i Event type to attach
	 * @param mod Module to attach event to
	 * @return True if the event was attached
	 */
	bool Attach(Implementation i, Module* mod);

	/** Detatch an event from a module.
	 * This is not required when your module unloads, as the core will
	 * automatically detatch your module from all events it is attached to.
	 * @param i Event type to detach
	 * @param mod Module to detach event from
	 * @param Detach true if the event was detached
	 */
	bool Detach(Implementation i, Module* mod);

	/** Attach an array of events to a module
	 * @param i Event types (array) to attach
	 * @param mod Module to attach events to
	 */
	void Attach(Implementation* i, Module* mod, size_t sz);

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
	 * @param state Module state from recently unloaded module
	 * @return True if the module was found and loaded
	 */
	bool Load(const std::string& filename, bool defer = false, ModuleState* state = NULL);

	/** Unload a given module file. Note that the module will not be
	 * completely gone until the cull list has finished processing.
	 *
	 * @return true on success; if false, LastError will give a reason
	 */
	bool Unload(Module* module);

	/** Run an asynchronous reload of the given module. When the reload is
	 * complete, the callback will be run with true if the reload succeeded
	 * and false if it did not.
	 */
	void Reload(Module* module, HandlerBase1<void, bool>* callback);

	/** Called by the InspIRCd constructor to load all modules from the config file.
	 */
	void LoadAll();
	void UnloadAll();
	void DoSafeUnload(Module*, ModuleState* state);
	void DoModuleLoad(Module*, ModuleState* state);

	/** Get the total number of currently loaded modules
	 * @return The number of loaded modules
	 */
	int GetCount() const
	{
		return this->ModCount;
	}

	/** Find a module by name, and return a Module* to it.
	 * This is preferred over iterating the module lists yourself.
	 * @param name The module name to look up
	 * @return A pointer to the module, or NULL if the module cannot be found
	 */
	Module* Find(const std::string &name);

	/** Register a service provided by a module */
	void AddService(ServiceProvider&);

	/** Unregister a service provided by a module */
	void DelService(ServiceProvider&);

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

	inline const std::map<std::string, Module*>& GetModules() const { return Modules; }

	/** Return a list of all modules matching the given filter
	 * @param filter This int is a bitmask of flags set in Module::Flags,
	 * such as VF_VENDOR or VF_STATIC. If you wish to receive a list of
	 * all modules with no filtering, set this to 0.
	 * @return The list of module names
	 */
	const std::vector<std::string> GetAllModuleNames(int filter);
};

class CoreExport CrashState : public interfacebase
{
 public:
	CrashState* parent;
	const char* const where;
	const void* const item;
	CrashState(const char* Where, const void* Item);
	~CrashState();
};

/** Do not mess with these functions unless you know the C preprocessor
 * well enough to explain why they are needed. The order is important.
 */
#define MODULE_INIT_STR MODULE_INIT_STR_FN_2(MODULE_INIT_SYM)
#define MODULE_INIT_STR_FN_2(x) MODULE_INIT_STR_FN_1(x)
#define MODULE_INIT_STR_FN_1(x) #x
#define MODULE_INIT_SYM MODULE_INIT_SYM_FN_2(INSPIRCD_VERSION_MAJ, INSPIRCD_VERSION_API)
#define MODULE_INIT_SYM_FN_2(x,y) MODULE_INIT_SYM_FN_1(x,y)
#define MODULE_INIT_SYM_FN_1(x,y) inspircd_module_ ## x ## _ ## y
#define HERE_STR HERE_STR_FN_2(__FILE__, __LINE__)
#define HERE_STR_FN_2(x,y) HERE_STR_FN_1(x,y)
#define HERE_STR_FN_1(x,y) " @" x ":" #y

#ifdef PURE_STATIC

struct AllCommandList {
	typedef Command* (*fn)(Module*);
	AllCommandList(fn cmd);
};
#define COMMAND_INIT(x) static Command* MK_ ## x(Module* m) { return new x(m); } \
	static const AllCommandList PREP_ ## x(&MK_ ## x);

struct AllModuleList {
	typedef Module* (*fn)();
	fn init;
	std::string name;
	AllModuleList(fn mod, const std::string& Name);
};

#define MODULE_INIT(x) static Module* MK_ ## x() { return new x; } \
	static const AllModuleList PREP_ ## x(&MK_ ## x, MODNAMESTR);

#define MODNAMESTR MODNAMESTR_FN_2(MODNAME)
#define MODNAMESTR_FN_2(x) MODNAMESTR_FN_1(x)
#define MODNAMESTR_FN_1(x) #x

#else

/** This definition is used as shorthand for the various classes
 * and functions needed to make a module loadable by the OS.
 * It defines the class factory and external init_module function.
 */
#ifdef WINDOWS

#define MODULE_INIT(y) \
	extern "C" DllExport Module * MODULE_INIT_SYM() \
	{ \
		return new y; \
	} \
	BOOLEAN WINAPI DllMain(HINSTANCE hDllHandle, DWORD nReason, LPVOID Reserved) \
	{ \
		switch ( nReason ) \
		{ \
			case DLL_PROCESS_ATTACH: \
			case DLL_PROCESS_DETACH: \
				break; \
		} \
		return TRUE; \
	}

#else

#define MODULE_INIT(y) \
	extern "C" DllExport Module * MODULE_INIT_SYM() \
	{ \
		return new y; \
	} \
	extern "C" const char inspircd_src_version[] = VERSION " r" REVISION;

extern "C" DllExport Module * MODULE_INIT_SYM();

#endif

#define COMMAND_INIT(c) MODULE_INIT(CommandModule<c>)

#endif

#endif
