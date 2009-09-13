/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __MODULES_H
#define __MODULES_H

#include "dynamic.h"
#include "base.h"
#include "ctables.h"
#include "inspsocket.h"
#include <string>
#include <deque>
#include <sstream>
#include "timer.h"
#include "mode.h"
#include "dns.h"

class XLine;

/** Used with OnAccessCheck() method of modules
 */
enum AccessControlType {
	ACR_DEFAULT,		// Do default action (act as if the module isnt even loaded)
	ACR_DENY,		// deny the action
	ACR_ALLOW,		// allow the action
	AC_KICK,		// a user is being kicked
	AC_DEOP,		// a user is being deopped
	AC_OP,			// a user is being opped
	AC_VOICE,		// a user is being voiced
	AC_DEVOICE,		// a user is being devoiced
	AC_HALFOP,		// a user is being halfopped
	AC_DEHALFOP,		// a user is being dehalfopped
	AC_INVITE,		// a user is being invited
	AC_GENERAL_MODE,	// a channel mode is being changed
	AC_GENERAL_UMODE	// a user mode is being changed
};

/** Used to define a set of behavior bits for a module
 */
enum ModuleFlags {
	VF_STATIC = 1,		// module is static, cannot be /unloadmodule'd
	VF_VENDOR = 2,		// module is a vendor module (came in the original tarball, not 3rd party)
	VF_SERVICEPROVIDER = 4,	// module provides a service to other modules (can be a dependency)
	VF_COMMON = 8,		// module needs to be common on all servers in a network to link
	VF_OPTCOMMON = 16	// module should be common on all servers for unsurprising behavior
};

/** Used with SendToMode()
 */
enum WriteModeFlags {
	WM_AND = 1,
	WM_OR = 2
};

/** Used to represent an event type, for user, channel or server
 */
enum TargetTypeFlags {
	TYPE_USER = 1,
	TYPE_CHANNEL,
	TYPE_SERVER,
	TYPE_OTHER
};

/** Used to represent wether a message was PRIVMSG or NOTICE
 */
enum MessageType {
	MSG_PRIVMSG = 0,
	MSG_NOTICE = 1
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
	bool operator==(const ModResult& r) const
	{
		return res == r.res;
	}
	bool operator!=(const ModResult& r) const
	{
		return res != r.res;
	}
	bool operator!() const
	{
		return !res;
	}
	bool check(bool def) const
	{
		return (res == 1 || (res == 0 && def));
	}
	/**
	 * Merges two results, preferring ALLOW to DENY
	 */
	ModResult operator+(const ModResult& r) const
	{
		if (res == r.res || r.res == 0)
			return *this;
		if (res == 0)
			return r;
		// they are different, and neither is passthru
		return MOD_RES_ALLOW;
	}
};

/** If you change the module API, change this value. */
#define API_VERSION 13000

class ServerConfig;

/* Forward-delacare module for ModuleMessage etc
 */
class Module;
class InspIRCd;

/** Low level definition of a FileReader classes file cache area -
 * a text file seperated into lines.
 */
typedef std::deque<std::string> file_cache;

/** A set of strings.
 */
typedef file_cache string_list;

/** Holds a list of 'published features' for modules.
 */
typedef std::map<std::string,Module*> featurelist;

/** Holds a list of modules which implement an interface
 */
typedef std::deque<Module*> modulelist;

/** Holds a list of all modules which implement interfaces, by interface name
 */
typedef std::map<std::string, std::pair<int, modulelist> > interfacelist;

/**
 * This #define allows us to call a method in all
 * loaded modules in a readable simple way, e.g.:
 * 'FOREACH_MOD(I_OnConnect,OnConnect(user));'
 */
#define FOREACH_MOD(y,x) do { \
	EventHandlerIter safei; \
	for (EventHandlerIter _i = ServerInstance->Modules->EventHandlers[y].begin(); _i != ServerInstance->Modules->EventHandlers[y].end(); ) \
	{ \
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
} while (0);

/**
 * This #define allows us to call a method in all
 * loaded modules in a readable simple way and pass
 * an instance pointer to the macro. e.g.:
 * 'FOREACH_MOD_I(Instance, OnConnect, OnConnect(user));'
 */
#define FOREACH_MOD_I(z,y,x) do { \
	EventHandlerIter safei; \
	for (EventHandlerIter _i = z->Modules->EventHandlers[y].begin(); _i != z->Modules->EventHandlers[y].end(); ) \
	{ \
		safei = _i; \
		++safei; \
		try \
		{ \
			(*_i)->x ; \
		} \
		catch (CoreException& modexcept) \
		{ \
			z->Logs->Log("MODULE",DEFAULT,"Exception caught: %s",modexcept.GetReason()); \
		} \
		_i = safei; \
	} \
} while (0);

/**
 * Custom module result handling loop. This is a paired macro, and should only
 * be used with while_each_hook.
 *
 * See src/channels.cpp for an example of use.
 */
#define DO_EACH_HOOK(z,n,v,args) \
do { \
	EventHandlerIter iter_ ## n = z->Modules->EventHandlers[I_ ## n].begin(); \
	while (iter_ ## n != z->Modules->EventHandlers[I_ ## n].end()) \
	{ \
		Module* mod_ ## n = *iter_ ## n; \
		iter_ ## n ++; \
		try \
		{ \
			v = (mod_ ## n)->n args;

#define WHILE_EACH_HOOK(z,n) \
		} \
		catch (CoreException& except_ ## n) \
		{ \
			z->Logs->Log("MODULE",DEFAULT,"Exception caught: %s", (except_ ## n).GetReason()); \
			(void) mod_ ## n; /* catch mismatched pairs */ \
		} \
	} \
} while(0)

/**
 * Module result iterator
 * Runs the given hook until some module returns a useful result.
 *
 * Example: ModResult result;
 * FIRST_MOD_RESULT(ServerInstance, OnUserPreNick, result, (user, newnick))
 */
#define FIRST_MOD_RESULT(z,n,v,args) do { \
	v = MOD_RES_PASSTHRU; \
	DO_EACH_HOOK(z,n,v,args) \
	{ \
		if (v != MOD_RES_PASSTHRU) \
			break; \
	} \
	WHILE_EACH_HOOK(z,n); \
} while (0)

/** Represents a non-local user.
 * (in fact, any FD less than -1 does)
 */
#define FD_MAGIC_NUMBER -42
/** Represents a fake user (i.e. a server)
 */
#define FD_FAKEUSER_NUMBER -7

/* Useful macros */

/** Is a local user */
#define IS_LOCAL(x) (x->GetFd() > -1)
/** Is a remote user */
#define IS_REMOTE(x) (x->GetFd() < 0)
/** Is a fake user */
#define IS_FAKE(x) (x->GetFd() == FD_FAKEUSER_NUMBER)
/** Is a module created user */
#define IS_MODULE_CREATED(x) (x->GetFd() == FD_MAGIC_NUMBER)
/** Is an oper */
#define IS_OPER(x) (!x->oper.empty())
/** Is away */
#define IS_AWAY(x) (!x->awaymsg.empty())

/** Holds a module's Version information.
 *  The four members (set by the constructor only) indicate details as to the version number
 *  of a module. A class of type Version is returned by the GetVersion method of the Module class.
 *  The flags and API values represent the module flags and API version of the module.
 *  The API version of a module must match the API version of the core exactly for the module to
 *  load successfully.
 */
class CoreExport Version : public classbase
{
 public:
	/** Version information.
	 */
	const std::string version;

	/** Flags and API version
	 */
	const int Flags, API;

	/** Initialize version class
	 */
	Version(const std::string &customver, int flags,
		int api_ver = API_VERSION, const std::string& src_rev = VERSION " r" REVISION);
};

/** The ModuleMessage class is the base class of Request and Event
 * This class is used to represent a basic data structure which is passed
 * between modules for safe inter-module communications.
 */
class CoreExport ModuleMessage : public Extensible
{
 public:
	/** Destructor
	 */
	virtual ~ModuleMessage() {};
};

/** The Request class is a unicast message directed at a given module.
 * When this class is properly instantiated it may be sent to a module
 * using the Send() method, which will call the given module's OnRequest
 * method with this class as its parameter.
 */
class CoreExport Request : public ModuleMessage
{
 protected:
	/** This member holds a pointer to arbitary data set by the emitter of the message
	 */
	char* data;
	/** This should be a null-terminated string identifying the type of request,
	 * all modules should define this and use it to determine the nature of the
	 * request before they attempt to cast the Request in any way.
	 */
	const char* id;
	/** This is a pointer to the sender of the message, which can be used to
	 * directly trigger events, or to create a reply.
	 */
	Module* source;
	/** The single destination of the Request
	 */
	Module* dest;
 public:
	/** Create a new Request
	 * This is for the 'old' way of casting whatever the data is
	 * to char* and hoping you get the right thing at the other end.
	 * This is slowly being depreciated in favor of the 'new' way.
	 */
	Request(char* anydata, Module* src, Module* dst);
	/** Create a new Request
	 * This is for the 'new' way of defining a subclass
	 * of Request and defining it in a common header,
	 * passing an object of your Request subclass through
	 * as a Request* and using the ID string to determine
	 * what to cast it back to and the other end. This is
	 * much safer as there are no casts not confirmed by
	 * the ID string, and all casts are child->parent and
	 * can be checked at runtime with dynamic_cast<>()
	 */
	Request(Module* src, Module* dst, const char* idstr);
	/** Fetch the Request data
	 */
	char* GetData();
	/** Fetch the ID string
	 */
	const char* GetId();
	/** Fetch the request source
	 */
	Module* GetSource();
	/** Fetch the request destination (should be 'this' in the receiving module)
	 */
	Module* GetDest();
	/** Send the Request.
	 * Upon returning the result will be arbitary data returned by the module you
	 * sent the request to. It is up to your module to know what this data is and
	 * how to deal with it.
	 */
	const char* Send();
};


/** The Event class is a unicast message directed at all modules.
 * When the class is properly instantiated it may be sent to all modules
 * using the Send() method, which will trigger the OnEvent method in
 * all modules passing the object as its parameter.
 */
class CoreExport Event : public ModuleMessage
{
 protected:
	/** This member holds a pointer to arbitary data set by the emitter of the message
	 */
	char* data;
	/** This is a pointer to the sender of the message, which can be used to
	 * directly trigger events, or to create a reply.
	 */
	Module* source;
	/** The event identifier.
	 * This is arbitary text which should be used to distinguish
	 * one type of event from another.
	 */
	std::string id;
 public:
	/** Create a new Event
	 */
	Event(char* anydata, Module* src, const std::string &eventid);
	/** Get the Event data
	 */
	char* GetData();
	/** Get the event Source
	 */
	Module* GetSource();
	/** Get the event ID.
	 * Use this to determine the event type for safe casting of the data
	 */
	std::string GetEventID();
	/** Send the Event.
	 * The return result of an Event::Send() will always be NULL as
	 * no replies are expected.
	 */
	char* Send(InspIRCd* ServerInstance);
};

/** Priority types which can be returned from Module::Prioritize()
 */
enum Priority { PRIORITY_FIRST, PRIORITY_DONTCARE, PRIORITY_LAST, PRIORITY_BEFORE, PRIORITY_AFTER };

/** Implementation-specific flags which may be set in Module::Implements()
 */
enum Implementation
{
	I_BEGIN,
	I_OnUserConnect, I_OnUserQuit, I_OnUserDisconnect, I_OnUserJoin, I_OnUserPart, I_OnRehash,
	I_OnSendSnotice, I_OnUserPreJoin, I_OnUserPreKick, I_OnUserKick, I_OnOper, I_OnInfo, I_OnWhois,
	I_OnUserPreInvite, I_OnUserInvite, I_OnUserPreMessage, I_OnUserPreNotice, I_OnUserPreNick,
	I_OnUserMessage, I_OnUserNotice, I_OnMode, I_OnGetServerDescription, I_OnSyncUser,
	I_OnSyncChannel, I_OnDecodeMetaData, I_OnWallops,
	I_OnChangeHost, I_OnChangeName, I_OnAddLine, I_OnDelLine, I_OnExpireLine, I_OnCleanup,
	I_OnUserPostNick, I_OnAccessCheck, I_On005Numeric, I_OnKill, I_OnRemoteKill, I_OnLoadModule,
	I_OnUnloadModule, I_OnBackgroundTimer, I_OnPreCommand, I_OnCheckReady, I_OnCheckInvite,
	I_OnRawMode, I_OnCheckKey, I_OnCheckLimit, I_OnCheckBan, I_OnCheckExtBan, I_OnCheckStringExtBan,
	I_OnStats, I_OnChangeLocalUserHost, I_OnChangeLocalUserGecos, I_OnPreTopicChange,
	I_OnPostTopicChange, I_OnEvent, I_OnRequest, I_OnGlobalOper, I_OnPostConnect, I_OnAddBan,
	I_OnDelBan, I_OnRawSocketAccept, I_OnRawSocketClose, I_OnRawSocketWrite, I_OnRawSocketRead,
	I_OnChangeLocalUserGECOS, I_OnUserRegister, I_OnChannelPreDelete, I_OnChannelDelete,
	I_OnPostOper, I_OnSyncNetwork, I_OnSetAway, I_OnUserList, I_OnPostCommand, I_OnPostJoin,
	I_OnWhoisLine, I_OnBuildExemptList, I_OnRawSocketConnect, I_OnGarbageCollect, I_OnBufferFlushed,
	I_OnText, I_OnPassCompare, I_OnRunTestSuite, I_OnNamesListItem, I_OnNumeric, I_OnHookIO,
	I_OnHostCycle, I_OnPreRehash, I_OnModuleRehash, I_OnSendWhoLine, I_OnChangeIdent,
	I_END
};

class ConfigReader;

/** Base class for all InspIRCd modules
 *  This class is the base class for InspIRCd modules. All modules must inherit from this class,
 *  its methods will be called when irc server events occur. class inherited from module must be
 *  instantiated by the ModuleFactory class (see relevent section) for the module to be initialised.
 */
class CoreExport Module : public Extensible
{
 public:
	/** Creator/owner pointer
	 */
	InspIRCd* ServerInstance;
	/** File that this module was loaded from
	 */
	std::string ModuleSourceFile;

	/** Default constructor.
	 * Creates a module class.
	 * @param Me An instance of the InspIRCd class which will be saved into ServerInstance for your use
	 * \exception ModuleException Throwing this class, or any class derived from ModuleException, causes loading of the module to abort.
	 */
	Module(InspIRCd* Me);

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
	virtual Version GetVersion();

	/** Called when a user connects.
	 * The details of the connecting user are available to you in the parameter User *user
	 * @param user The user who is connecting
	 */
	virtual void OnUserConnect(User* user);

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
	virtual void OnUserDisconnect(User* user);

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
	 * @param user The user who is joining
	 * @param channel The channel being joined
	 * @param silent Change this to true if you want to conceal the JOIN command from the other users
	 * of the channel (useful for modules such as auditorium)
	 * @param sync This is set to true if the JOIN is the result of a network sync and the remote user is being introduced
	 * to a channel due to the network sync.
	 * @param created This is true if the join created the channel
	 */
	virtual void OnUserJoin(User* user, Channel* channel, bool sync, bool &silent, bool created);

	/** Called after a user joins a channel
	 * Identical to OnUserJoin, but called immediately afterwards, when any linking module has
	 * seen the join.
	 * @param user The user who is joining
	 * @param channel The channel being joined
	 */
	virtual void OnPostJoin(User* user, Channel* channel);

	/** Called when a user parts a channel.
	 * The details of the leaving user are available to you in the parameter User *user,
	 * and the details of the channel they have left is available in the variable Channel *channel
	 * @param user The user who is parting
	 * @param channel The channel being parted
	 * @param partmessage The part message, or an empty string (may be modified)
	 * @param silent Change this to true if you want to conceal the PART command from the other users
	 * of the channel (useful for modules such as auditorium)
	 */
	virtual void OnUserPart(User* user, Channel* channel, std::string &partmessage, bool &silent);

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
	virtual void OnPreRehash(User* user, const std::string &parameter);

	/** Called on rehash.
	 * This method is called when a user initiates a module-specific rehash. This can be used to do
	 * expensive operations (such as reloading SSL certificates) that are not executed on a normal
	 * rehash for efficiency. A rehash of this type does not reload the core configuration.
	 *
	 * @param user The user performing the rehash.
	 * @param parameter The parameter given to REHASH
	 */
	virtual void OnModuleRehash(User* user, const std::string &parameter);

	/** Called on rehash.
	 * This method is called after a rehash has completed. You should use it to reload any module
	 * configuration from the main configuration file.
	 * @param user The user that performed the rehash, if it was initiated by a user and that user
	 * is still connected.
	 */
	virtual void OnRehash(User* user);

	/** Called whenever a snotice is about to be sent to a snomask.
	 * snomask and type may both be modified; the message may not.
	 * @param snomask The snomask the message is going to (e.g. 'A')
	 * @param type The textual description the snomask will go to (e.g. 'OPER')
	 * @param message The text message to be sent via snotice
	 * @return 1 to block the snotice from being sent entirely, 0 else.
	 */
	virtual ModResult OnSendSnotice(char &snomask, std::string &type, const std::string &message);

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
	 * @param privs A string containing the users privilages when joining the channel. For new channels this will contain "@".
	 * You may alter this string to alter the user's modes on the channel.
	 * @param keygiven The key given to join the channel, or an empty string if none was provided
	 * @return 1 To prevent the join, 0 to allow it.
	 */
	virtual ModResult OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven);

	/** Called whenever a user is about to be kicked.
	 * Returning a value of 1 from this function stops the process immediately, causing no
	 * output to be sent to the user by the core. If you do this you must produce your own numerics,
	 * notices etc.
	 * @param source The user issuing the kick
	 * @param user The user being kicked
	 * @param chan The channel the user is being kicked from
	 * @param reason The kick reason
	 * @return 1 to prevent the kick, 0 to continue normally, -1 to explicitly allow the kick regardless of normal operation
	 */
	virtual ModResult OnUserPreKick(User* source, User* user, Channel* chan, const std::string &reason);

	/** Called whenever a user is kicked.
	 * If this method is called, the kick is already underway and cannot be prevented, so
	 * to prevent a kick, please use Module::OnUserPreKick instead of this method.
	 * @param source The user issuing the kick
	 * @param user The user being kicked
	 * @param chan The channel the user is being kicked from
	 * @param reason The kick reason
	 * @param silent Change this to true if you want to conceal the PART command from the other users
	 * of the channel (useful for modules such as auditorium)
	 */
	virtual void OnUserKick(User* source, User* user, Channel* chan, const std::string &reason, bool &silent);

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
	virtual ModResult OnUserPreInvite(User* source,User* dest,Channel* channel, time_t timeout);

	/** Called after a user has been successfully invited to a channel.
	 * You cannot prevent the invite from occuring using this function, to do that,
	 * use OnUserPreInvite instead.
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

	/** Called whenever the server wants to build the exemption list for a channel, but is not directly doing a PRIVMSG or NOTICE.
	 * For example, the spanningtree protocol will call this event when passing a privmsg on (but not processing it directly).
	 * @param message_type The message type, either MSG_PRIVMSG or MSG_NOTICE
	 * @param chan The channel to build the exempt list of
	 * @param sender The original sender of the PRIVMSG or NOTICE
	 * @param status The status char to be used for the channel list
	 * @param exempt_list The exempt list to be populated
	 * @param text The original message text causing the exempt list to be built
	 */
	virtual void OnBuildExemptList(MessageType message_type, Channel* chan, User* sender, char status, CUList &exempt_list, const std::string &text);

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
	 * The dest variable contains a User* if target_type is TYPE_USER and a Channel*
	 * if target_type is TYPE_CHANNEL. The text variable contains the remainder of the
	 * mode string after the target, e.g. "+wsi" or "+ooo nick1 nick2 nick3".
	 * @param user The user sending the MODEs
	 * @param dest The target of the modes (User* or Channel*)
	 * @param target_type The type of target (TYPE_USER or TYPE_CHANNEL)
	 * @param text The actual modes and their parameters if any
	 * @param translate The translation types of the mode parameters
	 */
	virtual void OnMode(User* user, void* dest, int target_type, const std::vector<std::string> &text, const std::vector<TranslateType> &translate);

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
	 * @param proto A pointer to the module handling network protocol
	 * @param opaque An opaque pointer set by the protocol module, should not be modified!
	 */
	virtual void OnSyncUser(User* user, Module* proto, void* opaque);

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
	 * @param opaque An opaque pointer set by the protocol module, should not be modified!
	 */
	virtual void OnSyncChannel(Channel* chan, Module* proto, void* opaque);

	/* Allows modules to syncronize metadata not related to users or channels, over the network during a netburst.
	 * Whenever the linking module wants to send out data, but doesnt know what the data
	 * represents (e.g. it is Extensible metadata, added to a User or Channel by a module) then
	 * this method is called. You should use the ProtoSendMetaData function after you've
	 * correctly decided how the data should be represented, to send the metadata on its way if
	 * if it belongs to your module.
	 * @param proto A pointer to the module handling network protocol
	 * @param opaque An opaque pointer set by the protocol module, should not be modified!
	 * @param displayable If this value is true, the data is going to be displayed to a user,
	 * and not sent across the network. Use this to determine wether or not to show sensitive data.
	 */
	virtual void OnSyncNetwork(Module* proto, void* opaque);

	/** Allows module data, sent via ProtoSendMetaData, to be decoded again by a receiving module.
	 * Please see src/modules/m_swhois.cpp for a working example of how to use this method call.
	 * @param target_type The type of item to decode data for, TYPE_USER or TYPE_CHANNEL
	 * @param target The Channel* or User* that data should be added to
	 * @param extname The extension name which is being sent
	 * @param extdata The extension data, encoded at the other end by an identical module through OnSyncChannelMetaData or OnSyncUserMetaData
	 */
	virtual void OnDecodeMetaData(Extensible* target, const std::string &extname, const std::string &extdata);

	/** Implemented by modules which provide the ability to link servers.
	 * These modules will implement this method, which allows transparent sending of servermodes
	 * down the network link as a broadcast, without a module calling it having to know the format
	 * of the MODE command before the actual mode string.
	 *
	 * More documentation to follow soon. Please see src/modules/m_chanprotect.cpp for examples
	 * of how to use this function.
	 *
	 * @param opaque An opaque pointer set by the protocol module, should not be modified!
	 * @param target_type The type of item to decode data for, TYPE_USER or TYPE_CHANNEL
	 * @param target The Channel* or User* that modes should be sent for
	 * @param modeline The modes and parameters to be sent
	 * @param translate The translation types of the mode parameters
	 */
	virtual void ProtoSendMode(void* opaque, TargetTypeFlags target_type, void* target, const std::vector<std::string> &modeline, const std::vector<TranslateType> &translate);

	/** Implemented by modules which provide the ability to link servers.
	 * These modules will implement this method, which allows metadata (extra data added to
	 * user and channel records using class Extensible, Extensible::Extend, etc) to be sent
	 * to other servers on a netburst and decoded at the other end by the same module on a
	 * different server.
	 *
	 * More documentation to follow soon. Please see src/modules/m_swhois.cpp for example of
	 * how to use this function.
	 * @param opaque An opaque pointer set by the protocol module, should not be modified!
	 * @param target_type The type of item to decode data for, TYPE_USER or TYPE_CHANNEL
	 * @param target The Channel* or User* that metadata should be sent for
	 * @param extname The extension name to send metadata for
	 * @param extdata Encoded data for this extension name, which will be encoded at the oppsite end by an identical module using OnDecodeMetaData
	 */
	virtual void ProtoSendMetaData(void* opaque, Extensible* target, const std::string &extname, const std::string &extdata);

	/**
	 * Implemented by all modules that implement ProtoSendMetaData.
	 * Translates the item into a string format suitable for sending to other servers.
	 * Currently, this just translates nicks to their UID and channels to their name
	 */
	virtual std::string ProtoTranslate(Extensible* item);

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

	/** Called whenever an xline is deleted MANUALLY. See OnExpireLine for expiry.
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

	/** Called before an action which requires a channel privilage check.
	 * This function is called before many functions which check a users status on a channel, for example
	 * before opping a user, deopping a user, kicking a user, etc.
	 * There are several values for access_type which indicate for what reason access is being checked.
	 * These are:<br><br>
	 * AC_KICK - A user is being kicked<br>
	 * AC_DEOP - a user is being deopped<br>
	 * AC_OP - a user is being opped<br>
	 * AC_VOICE - a user is being voiced<br>
	 * AC_DEVOICE - a user is being devoiced<br>
	 * AC_HALFOP - a user is being halfopped<br>
	 * AC_DEHALFOP - a user is being dehalfopped<br>
	 * AC_INVITE - a user is being invited<br>
	 * AC_GENERAL_MODE - a user channel mode is being changed<br><br>
	 * Upon returning from your function you must return either ACR_DEFAULT, to indicate the module wishes
	 * to do nothing, or ACR_DENY where approprate to deny the action, and ACR_ALLOW where appropriate to allow
	 * the action. Please note that in the case of some access checks (such as AC_GENERAL_MODE) access may be
	 * denied 'upstream' causing other checks such as AC_DEOP to not be reached. Be very careful with use of the
	 * AC_GENERAL_MODE type, as it may inadvertently override the behaviour of other modules. When the access_type
	 * is AC_GENERAL_MODE, the destination of the mode will be NULL (as it has not yet been determined).
	 * @param source The source of the access check
	 * @param dest The destination of the access check
	 * @param channel The channel which is being checked
	 * @param access_type See above
	 */
	virtual ModResult OnAccessCheck(User* source,User* dest,Channel* channel,int access_type);

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
	 * @param name The new module's filename
	 */
	virtual void OnLoadModule(Module* mod,const std::string &name);

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
	virtual void OnUnloadModule(Module* mod,const std::string &name);

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
	virtual ModResult OnPreCommand(std::string &command, std::vector<std::string>& parameters, User *user, bool validated, const std::string &original_line);

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
	virtual void OnPostCommand(const std::string &command, const std::vector<std::string>& parameters, User *user, CmdResult result, const std::string &original_line);

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
	virtual ModResult OnCheckReady(User* user);

	/** Called whenever a user is about to register their connection (e.g. before the user
	 * is sent the MOTD etc). Modules can use this method if they are performing a function
	 * which must be done before the actual connection is completed (e.g. ident lookups,
	 * dnsbl lookups, etc).
	 * Note that you should NOT delete the user record here by causing a disconnection!
	 * Use OnUserConnect for that instead.
	 * @param user The user registering
	 * @return 1 to indicate user quit, 0 to continue
	 */
	virtual ModResult OnUserRegister(User* user);

	/** Called whenever a user joins a channel, to determine if invite checks should go ahead or not.
	 * This method will always be called for each join, wether or not the channel is actually +i, and
	 * determines the outcome of an if statement around the whole section of invite checking code.
	 * return 1 to explicitly allow the join to go ahead or 0 to ignore the event.
	 * @param user The user joining the channel
	 * @param chan The channel being joined
	 * @return 1 to explicitly allow the join, 0 to proceed as normal
	 */
	virtual ModResult OnCheckInvite(User* user, Channel* chan);

	/** Called whenever a mode character is processed.
	 * Return 1 from this function to block the mode character from being processed entirely.
	 * @param user The user who is sending the mode
	 * @param chan The channel the mode is being sent to (or NULL if a usermode)
	 * @param mode The mode character being set
	 * @param param The parameter for the mode or an empty string
	 * @param adding true of the mode is being added, false if it is being removed
	 * @param pcnt The parameter count for the mode (0 or 1)
	 * @return ACR_DENY to deny the mode, ACR_DEFAULT to do standard mode checking, and ACR_ALLOW
	 * to skip all permission checking. Please note that for remote mode changes, your return value
	 * will be ignored!
	 */
	virtual ModResult OnRawMode(User* user, Channel* chan, const char mode, const std::string &param, bool adding, int pcnt);

	/** Called whenever a user joins a channel, to determine if key checks should go ahead or not.
	 * This method will always be called for each join, wether or not the channel is actually +k, and
	 * determines the outcome of an if statement around the whole section of key checking code.
	 * if the user specified no key, the keygiven string will be a valid but empty value.
	 * return 1 to explicitly allow the join to go ahead or 0 to ignore the event.
	 * @param user The user joining the channel
	 * @param chan The channel being joined
	 * @return 1 to explicitly allow the join, 0 to proceed as normal
	 */
	virtual ModResult OnCheckKey(User* user, Channel* chan, const std::string &keygiven);

	/** Called whenever a user joins a channel, to determine if channel limit checks should go ahead or not.
	 * This method will always be called for each join, wether or not the channel is actually +l, and
	 * determines the outcome of an if statement around the whole section of channel limit checking code.
	 * return 1 to explicitly allow the join to go ahead or 0 to ignore the event.
	 * @param user The user joining the channel
	 * @param chan The channel being joined
	 * @return 1 to explicitly allow the join, 0 to proceed as normal
	 */
	virtual ModResult OnCheckLimit(User* user, Channel* chan);

	/** Called whenever a user joins a channel, to determine if banlist checks should go ahead or not.
	 * This method will always be called for each join, wether or not the user actually matches a channel ban, and
	 * determines the outcome of an if statement around the whole section of ban checking code.
	 * return 1 to explicitly allow the join to go ahead or 0 to ignore the event.
	 * @param user The user joining the channel
	 * @param chan The channel being joined
	 * @return 1 to explicitly allow the join, 0 to proceed as normal. Return -1 to explicitly deny the
	 * join to the channel.
	 */
	virtual ModResult OnCheckBan(User* user, Channel* chan);

	/* Called whenever checking whether or not a user is matched by an applicable extended bantype.
	 * NOTE: may also trigger extra OnCheckStringExtBan events!
	 * @param u The user to check
	 * @param c The channel the user is on
	 * @param type The type of extended ban to check for.
	 * @returns 1 = exempt, 0 = no match, -1 = banned
	 */
	virtual ModResult OnCheckExtBan(User *u, Channel *c, char type);

	/** Called whenever checking whether or not a string is extbanned. NOTE: one OnCheckExtBan will also trigger a number of
	 * OnCheckStringExtBan events for seperate host/IP comnbinations.
	 * @returns 1 = exempt, 0 = no match, -1 = banned
	 */
	virtual ModResult OnCheckStringExtBan(const std::string &s, Channel *c, char type);

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

	/** Called whenever a change of a local users displayed host is attempted.
	 * Return 1 to deny the host change, or 0 to allow it.
	 * @param user The user whos host will be changed
	 * @param newhost The new hostname
	 * @return 1 to deny the host change, 0 to allow
	 */
	virtual ModResult OnChangeLocalUserHost(User* user, const std::string &newhost);

	/** Called whenever a change of a local users GECOS (fullname field) is attempted.
	 * return 1 to deny the name change, or 0 to allow it.
	 * @param user The user whos GECOS will be changed
	 * @param newhost The new GECOS
	 * @return 1 to deny the GECOS change, 0 to allow
	 */
	virtual ModResult OnChangeLocalUserGECOS(User* user, const std::string &newhost);

	/** Called before a topic is changed.
	 * Return 1 to deny the topic change, 0 to check details on the change, -1 to let it through with no checks
	 * As with other 'pre' events, you should only ever block a local event.
	 * @param user The user changing the topic
	 * @param chan The channels who's topic is being changed
	 * @param topic The actual topic text
	 * @param 1 to block the topic change, 0 to allow
	 */
	virtual ModResult OnPreTopicChange(User* user, Channel* chan, const std::string &topic);

	/** Called whenever a topic has been changed.
	 * To block topic changes you must use OnPreTopicChange instead.
	 * @param user The user changing the topic
	 * @param chan The channels who's topic is being changed
	 * @param topic The actual topic text
	 */
	virtual void OnPostTopicChange(User* user, Channel* chan, const std::string &topic);

	/** Called whenever an Event class is sent to all module by another module.
	 * Please see the documentation of Event::Send() for further information. The Event sent can
	 * always be assumed to be non-NULL, you should *always* check the value of Event::GetEventID()
	 * before doing anything to the event data, and you should *not* change the event data in any way!
	 * @param event The Event class being received
	 */
	virtual void OnEvent(Event* event);

	/** Called whenever a Request class is sent to your module by another module.
	 * Please see the documentation of Request::Send() for further information. The Request sent
	 * can always be assumed to be non-NULL, you should not change the request object or its data.
	 * Your method may return arbitary data in the char* result which the requesting module
	 * may be able to use for pre-determined purposes (e.g. the results of an SQL query, etc).
	 * @param request The Request class being received
	 */
	virtual const char* OnRequest(Request* request);

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

	/** Called whenever a user is given usermode +o, anywhere on the network.
	 * You cannot override this and prevent it from happening as it is already happened and
	 * such a task must be performed by another server. You can however bounce modes by sending
	 * servermodes out to reverse mode changes.
	 * @param user The user who is opering
	 */
	virtual void OnGlobalOper(User* user);

	/** Called after a user has fully connected and all modules have executed OnUserConnect
	 * This event is informational only. You should not change any user information in this
	 * event. To do so, use the OnUserConnect method to change the state of local users.
	 * This is called for both local and remote users.
	 * @param user The user who is connecting
	 */
	virtual void OnPostConnect(User* user);

	/** Called whenever a ban is added to a channel's list.
	 * Return a non-zero value to 'eat' the mode change and prevent the ban from being added.
	 * @param source The user adding the ban
	 * @param channel The channel the ban is being added to
	 * @param banmask The ban mask being added
	 * @return 1 to block the ban, 0 to continue as normal
	 */
	virtual ModResult OnAddBan(User* source, Channel* channel,const std::string &banmask);

	/** Called whenever a ban is removed from a channel's list.
	 * Return a non-zero value to 'eat' the mode change and prevent the ban from being removed.
	 * @param source The user deleting the ban
	 * @param channel The channel the ban is being deleted from
	 * @param banmask The ban mask being deleted
	 * @return 1 to block the unban, 0 to continue as normal
	 */
	virtual ModResult OnDelBan(User* source, Channel* channel,const std::string &banmask);

	/** Called to install an I/O hook on an event handler
	 * @param user The item to possibly install the I/O hook on
	 * @param via The port that <user> connected on
	 */
	virtual void OnHookIO(EventHandler* user, ListenSocketBase* via);

	/** Called immediately after any  connection is accepted. This is intended for raw socket
	 * processing (e.g. modules which wrap the tcp connection within another library) and provides
	 * no information relating to a user record as the connection has not been assigned yet.
	 * There are no return values from this call as all modules get an opportunity if required to
	 * process the connection.
	 * @param fd The file descriptor returned from accept()
	 * @param client The client IP address and port
	 * @param server The server IP address and port
	 * @param localport The local port number the user connected to
	 */
	virtual void OnRawSocketAccept(int fd, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server);

	/** Called immediately before any write() operation on a user's socket in the core. Because
	 * this event is a low level event no user information is associated with it. It is intended
	 * for use by modules which may wrap connections within another API such as SSL for example.
	 * return a non-zero result if you have handled the write operation, in which case the core
	 * will not call write().
	 * @param fd The file descriptor of the socket
	 * @param buffer A char* buffer being written
	 * @param Number of characters to write
	 * @return Number of characters actually written or 0 if you didn't handle the operation
	 */
	virtual int OnRawSocketWrite(int fd, const char* buffer, int count);

	/** Called immediately before any socket is closed. When this event is called, shutdown()
	 * has not yet been called on the socket.
	 * @param fd The file descriptor of the socket prior to close()
	 */
	virtual void OnRawSocketClose(int fd);

	/** Called immediately upon connection of an outbound BufferedSocket which has been hooked
	 * by a module.
	 * @param fd The file descriptor of the socket immediately after connect()
	 */
	virtual void OnRawSocketConnect(int fd);

	/** Called immediately before any read() operation on a client socket in the core.
	 * This occurs AFTER the select() or poll() so there is always data waiting to be read
	 * when this event occurs.
	 * Your event should return 1 if it has handled the reading itself, which prevents the core
	 * just using read(). You should place any data read into buffer, up to but NOT GREATER THAN
	 * the value of count. The value of readresult must be identical to an actual result that might
	 * be returned from the read() system call, for example, number of bytes read upon success,
	 * 0 upon EOF or closed socket, and -1 for error. If your function returns a nonzero value,
	 * you MUST set readresult.
	 * @param fd The file descriptor of the socket
	 * @param buffer A char* buffer being read to
	 * @param count The size of the buffer
	 * @param readresult The amount of characters read, or 0
	 * @return nonzero if the event was handled, in which case readresult must be valid on exit
	 */
	virtual int OnRawSocketRead(int fd, char* buffer, unsigned int count, int &readresult);

	/** Called whenever a user sets away or returns from being away.
	 * The away message is available as a parameter, but should not be modified.
	 * At this stage, it has already been copied into the user record.
	 * If awaymsg is empty, the user is returning from away.
	 * @param user The user setting away
	 * @param awaymsg The away message of the user, or empty if returning from away
	 * @return nonzero if the away message should be blocked - should ONLY be nonzero for LOCAL users (IS_LOCAL) (no output is returned by core)
	 */
	virtual ModResult OnSetAway(User* user, const std::string &awaymsg);

	/** Called whenever a NAMES list is requested.
	 * You can produce the nameslist yourself, overriding the current list,
	 * and if you do you must return 1. If you do not handle the names list,
	 * return 0.
	 * @param The user requesting the NAMES list
	 * @param Ptr The channel the NAMES list is requested for
	 */
	virtual ModResult OnUserList(User* user, Channel* Ptr);

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

	/** Called whenever a user's write buffer has been completely sent.
	 * This is called when the user's write buffer is completely empty, and
	 * there are no more pending bytes to be written and no pending write events
	 * in the socket engine's queue. This may be used to refill the buffer with
	 * data which is being spooled in a controlled manner, e.g. LIST lines.
	 * @param user The user who's buffer is now empty.
	 */
	virtual void OnBufferFlushed(User* user);

	/** Add test suite hooks here. These are used for testing functionality of a module
	 * via the --testsuite debugging parameter.
	 */
	virtual void OnRunTestSuite();

	/** Called for every item in a NAMES list, so that modules may reformat portions of it as they see fit.
	 * For example NAMESX, channel mode +u and +I, and UHNAMES. If the nick is set to an empty string by any
	 * module, then this will cause the nickname not to be displayed at all.
	 */
	virtual void OnNamesListItem(User* issuer, User* user, Channel* channel, std::string &prefixes, std::string &nick);

	virtual ModResult OnNumeric(User* user, unsigned int numeric, const std::string &text);

	/** Called for every time the user's host or ident changes, to indicate wether or not the 'Changing host'
	 * message should be sent, if enabled. Certain modules such as auditorium may opt to hide this message
	 * even if it is enabled.
	 */
	virtual ModResult OnHostCycle(User* user);

	/** Called whenever a result from /WHO is about to be returned
	 * @param source The user running the /WHO query
	 * @param user The user that this line of the query is about
	 * @param channel The channel being queried (or NULL if not a channel query)
	 * @param line The raw line to send; modifiable, if empty no line will be returned.
	 */
	virtual void OnSendWhoLine(User* source, User* user, Channel* channel, std::string& line);
};


#define CONF_NO_ERROR		0x000000
#define CONF_NOT_A_NUMBER	0x000010
#define CONF_INT_NEGATIVE	0x000080
#define CONF_VALUE_NOT_FOUND	0x000100
#define CONF_FILE_NOT_FOUND	0x000200


/** Allows reading of values from configuration files
 * This class allows a module to read from either the main configuration file (inspircd.conf) or from
 * a module-specified configuration file. It may either be instantiated with one parameter or none.
 * Constructing the class using one parameter allows you to specify a path to your own configuration
 * file, otherwise, inspircd.conf is read.
 */
class CoreExport ConfigReader : public classbase
{
  protected:
	InspIRCd* ServerInstance;
	/** Error code
	 */
	long error;

  public:
	/** Default constructor.
	 * This constructor initialises the ConfigReader class to read the inspircd.conf file
	 * as specified when running ./configure.
	 */
	ConfigReader(InspIRCd* Instance);
	/** Default destructor.
	 * This method destroys the ConfigReader class.
	 */
	~ConfigReader();

	/** Retrieves a value from the config file.
	 * This method retrieves a value from the config file. Where multiple copies of the tag
	 * exist in the config file, index indicates which of the values to retrieve.
	 */
	std::string ReadValue(const std::string &tag, const std::string &name, int index, bool allow_linefeeds = false);
	/** Retrieves a value from the config file.
	 * This method retrieves a value from the config file. Where multiple copies of the tag
	 * exist in the config file, index indicates which of the values to retrieve. If the
	 * tag is not found the default value is returned instead.
	 */
	std::string ReadValue(const std::string &tag, const std::string &name, const std::string &default_value, int index, bool allow_linefeeds = false);

	/** Retrieves a boolean value from the config file.
	 * This method retrieves a boolean value from the config file. Where multiple copies of the tag
	 * exist in the config file, index indicates which of the values to retrieve. The values "1", "yes"
	 * and "true" in the config file count as true to ReadFlag, and any other value counts as false.
	 */
	bool ReadFlag(const std::string &tag, const std::string &name, int index);
	/** Retrieves a boolean value from the config file.
	 * This method retrieves a boolean value from the config file. Where multiple copies of the tag
	 * exist in the config file, index indicates which of the values to retrieve. The values "1", "yes"
	 * and "true" in the config file count as true to ReadFlag, and any other value counts as false.
	 * If the tag is not found, the default value is used instead.
	 */
	bool ReadFlag(const std::string &tag, const std::string &name, const std::string &default_value, int index);

	/** Retrieves an integer value from the config file.
	 * This method retrieves an integer value from the config file. Where multiple copies of the tag
	 * exist in the config file, index indicates which of the values to retrieve. Any invalid integer
	 * values in the tag will cause the objects error value to be set, and any call to GetError() will
	 * return CONF_INVALID_NUMBER to be returned. need_positive is set if the number must be non-negative.
	 * If a negative number is placed into a tag which is specified positive, 0 will be returned and GetError()
	 * will return CONF_INT_NEGATIVE. Note that need_positive is not suitable to get an unsigned int - you
	 * should cast the result to achieve that effect.
	 */
	int ReadInteger(const std::string &tag, const std::string &name, int index, bool need_positive);
	/** Retrieves an integer value from the config file.
	 * This method retrieves an integer value from the config file. Where multiple copies of the tag
	 * exist in the config file, index indicates which of the values to retrieve. Any invalid integer
	 * values in the tag will cause the objects error value to be set, and any call to GetError() will
	 * return CONF_INVALID_NUMBER to be returned. needs_unsigned is set if the number must be unsigned.
	 * If a signed number is placed into a tag which is specified unsigned, 0 will be returned and GetError()
	 * will return CONF_NOT_UNSIGNED. If the tag is not found, the default value is used instead.
	 */
	int ReadInteger(const std::string &tag, const std::string &name, const std::string &default_value, int index, bool need_positive);

	/** Returns the last error to occur.
	 * Valid errors can be found by looking in modules.h. Any nonzero value indicates an error condition.
	 * A call to GetError() resets the error flag back to 0.
	 */
	long GetError();
	/** Counts the number of times a given tag appears in the config file.
	 * This method counts the number of times a tag appears in a config file, for use where
	 * there are several tags of the same kind, e.g. with opers and connect types. It can be
	 * used with the index value of ConfigReader::ReadValue to loop through all copies of a
	 * multiple instance tag.
	 */
	int Enumerate(const std::string &tag);

	/** Returns the number of items within a tag.
	 * For example if the tag was &lt;test tag="blah" data="foo"&gt; then this
	 * function would return 2. Spaces and newlines both qualify as valid seperators
	 * between values.
	 */
	int EnumerateValues(const std::string &tag, int index);
};



/** Caches a text file into memory and can be used to retrieve lines from it.
 * This class contains methods for read-only manipulation of a text file in memory.
 * Either use the constructor type with one parameter to load a file into memory
 * at construction, or use the LoadFile method to load a file.
 */
class CoreExport FileReader : public classbase
{
	InspIRCd* ServerInstance;
	/** The file contents
	 */
	file_cache fc;

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
	FileReader(InspIRCd* Instance);

	/** Secondary constructor.
	 * This method initialises the class with a file loaded into it ready for GetLine and
	 * and other methods to be called. If the file could not be loaded, FileReader::FileSize
	 * returns 0.
	 */
	FileReader(InspIRCd* Instance, const std::string &filename);

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

/** A DLLFactory (designed to load shared objects) containing a
 * handle to a module's init_module() function. Unfortunately,
 * due to the design of shared object systems we must keep this
 * hanging around, as if we remove this handle, we remove the
 * shared object file from memory (!)
 */
typedef DLLFactory<Module> ircd_module;

/** A list of modules
 */
typedef std::vector<Module*> IntModuleList;

/** An event handler iterator
 */
typedef IntModuleList::iterator EventHandlerIter;

/** ModuleManager takes care of all things module-related
 * in the core.
 */
class CoreExport ModuleManager : public classbase
{
 private:
	/** Holds a string describing the last module error to occur
	 */
	std::string LastModuleError;

	/** The feature names published by various modules
	 */
	featurelist Features;

	/** The interface names published by various modules
	 */
	interfacelist Interfaces;

	/** Total number of modules loaded into the ircd
	 */
	int ModCount;

	/** Our pointer to the main insp instance
	 */
	InspIRCd* Instance;

	/** List of loaded modules and shared object/dll handles
	 * keyed by module name
	 */
	std::map<std::string, std::pair<ircd_module*, Module*> > Modules;

	enum {
		PRIO_STATE_FIRST,
		PRIO_STATE_AGAIN,
		PRIO_STATE_LAST
	} prioritizationState;
 public:

	/** Event handler hooks.
	 * This needs to be public to be used by FOREACH_MOD and friends.
	 */
	IntModuleList EventHandlers[I_END];

	/** Simple, bog-standard, boring constructor.
	 */
	ModuleManager(InspIRCd* Ins);

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
	 * @param modules If PRIO_BEFORE or PRIORITY_AFTER is set in parameter 's',
	 * then this contains a list of one or more modules your module must be
	 * placed before or after. Your module will be placed before the highest
	 * priority module in this list for PRIO_BEFORE, or after the lowest
	 * priority module in this list for PRIORITY_AFTER.
	 * @param sz The number of modules being passed for PRIO_BEFORE and PRIORITY_AFTER.
	 * Defaults to 1, as most of the time you will only want to prioritize your module
	 * to be before or after one other module.
	 */
	bool SetPriority(Module* mod, Implementation i, Priority s, Module** modules = NULL, size_t sz = 1);

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
	 * @return True if the module was found and loaded
	 */
	bool Load(const char* filename);

	/** Unload a given module file
	 * @param filename The file to unload
	 * @return True if the module was unloaded
	 */
	bool Unload(const char* filename);

	/** Called by the InspIRCd constructor to load all modules from the config file.
	 */
	void LoadAll();

	/** Get the total number of currently loaded modules
	 * @return The number of loaded modules
	 */
	int GetCount()
	{
		return this->ModCount;
	}

	/** Find a module by name, and return a Module* to it.
	 * This is preferred over iterating the module lists yourself.
	 * @param name The module name to look up
	 * @return A pointer to the module, or NULL if the module cannot be found
	 */
	Module* Find(const std::string &name);

	/** Publish a 'feature'.
	 * There are two ways for a module to find another module it depends on.
	 * Either by name, using InspIRCd::FindModule, or by feature, using this
	 * function. A feature is an arbitary string which identifies something this
	 * module can do. For example, if your module provides SSL support, but other
	 * modules provide SSL support too, all the modules supporting SSL should
	 * publish an identical 'SSL' feature. This way, any module requiring use
	 * of SSL functions can just look up the 'SSL' feature using FindFeature,
	 * then use the module pointer they are given.
	 * @param FeatureName The case sensitive feature name to make available
	 * @param Mod a pointer to your module class
	 * @returns True on success, false if the feature is already published by
	 * another module.
	 */
	bool PublishFeature(const std::string &FeatureName, Module* Mod);

	/** Publish a module to an 'interface'.
	 * Modules which implement the same interface (the same way of communicating
	 * with other modules) can publish themselves to an interface, using this
	 * method. When they do so, they become part of a list of related or
	 * compatible modules, and a third module may then query for that list
	 * and know that all modules within that list offer the same API.
	 * A prime example of this is the hashing modules, which all accept the
	 * same types of Request class. Consider this to be similar to PublishFeature,
	 * except for that multiple modules may publish the same 'feature'.
	 * @param InterfaceName The case sensitive interface name to make available
	 * @param Mod a pointer to your module class
	 * @returns True on success, false on failure (there are currently no failure
	 * cases)
	 */
	bool PublishInterface(const std::string &InterfaceName, Module* Mod);

	/** Return a pair saying how many other modules are currently using the
	 * interfaces provided by module m.
	 * @param m The module to count usage for
	 * @return A pair, where the first value is the number of uses of the interface,
	 * and the second value is the interface name being used.
	 */
	std::pair<int,std::string> GetInterfaceInstanceCount(Module* m);

	/** Mark your module as using an interface.
	 * If you mark your module as using an interface, then that interface
	 * module may not unload until your module has unloaded first.
	 * This can be used to prevent crashes by ensuring code you depend on
	 * is always in memory while your module is active.
	 * @param InterfaceName The interface to use
	 */
	void UseInterface(const std::string &InterfaceName);

	/** Mark your module as finished with an interface.
	 * If you used UseInterface() above, you should use this method when
	 * your module is finished with the interface (usually in its destructor)
	 * to allow the modules which implement the given interface to be unloaded.
	 * @param InterfaceName The interface you are finished with using.
	 */
	void DoneWithInterface(const std::string &InterfaceName);

	/** Unpublish a 'feature'.
	 * When your module exits, it must call this method for every feature it
	 * is providing so that the feature table is cleaned up.
	 * @param FeatureName the feature to remove
	 */
	bool UnpublishFeature(const std::string &FeatureName);

	/** Unpublish your module from an interface
	 * When your module exits, it must call this method for every interface
	 * it is part of so that the interfaces table is cleaned up. Only when
	 * the last item is deleted from an interface does the interface get
	 * removed.
	 * @param InterfaceName the interface to be removed from
	 * @param Mod The module to remove from the interface list
	 */
	bool UnpublishInterface(const std::string &InterfaceName, Module* Mod);

	/** Find a 'feature'.
	 * There are two ways for a module to find another module it depends on.
	 * Either by name, using InspIRCd::FindModule, or by feature, using the
	 * InspIRCd::PublishFeature method. A feature is an arbitary string which
	 * identifies something this module can do. For example, if your module
	 * provides SSL support, but other modules provide SSL support too, all
	 * the modules supporting SSL should publish an identical 'SSL' feature.
	 * To find a module capable of providing the feature you want, simply
	 * call this method with the feature name you are looking for.
	 * @param FeatureName The feature name you wish to obtain the module for
	 * @returns A pointer to a valid module class on success, NULL on failure.
	 */
	Module* FindFeature(const std::string &FeatureName);

	/** Find an 'interface'.
	 * An interface is a list of modules which all implement the same API.
	 * @param InterfaceName The Interface you wish to obtain the module
	 * list of.
	 * @return A pointer to a deque of Module*, or NULL if the interface
	 * does not exist.
	 */
	modulelist* FindInterface(const std::string &InterfaceName);

	/** Determine if a module has published the named interface.
	 * This could be used in, for example, OnLoadModule to pick up other modules that can be used.
	 * @param mod The module to check.
	 * @param InterfaceName the interface you want to check for
	 * @return True if the module provides the interface, false otherwise.
	 */
	bool ModuleHasInterface(Module* mod, const std::string& InterfaceName);

	/** Given a pointer to a Module, return its filename
	 * @param m The module pointer to identify
	 * @return The module name or an empty string
	 */
	const std::string& GetModuleName(Module* m);

	/** Return a list of all modules matching the given filter
	 * @param filter This int is a bitmask of flags set in Module::Flags,
	 * such as VF_VENDOR or VF_STATIC. If you wish to receive a list of
	 * all modules with no filtering, set this to 0.
	 * @return The list of module names
	 */
	const std::vector<std::string> GetAllModuleNames(int filter);
};

/** This definition is used as shorthand for the various classes
 * and functions needed to make a module loadable by the OS.
 * It defines the class factory and external init_module function.
 */
#ifdef WINDOWS

#define MODULE_INIT(y) \
	extern "C" DllExport Module * init_module(InspIRCd* Me) \
	{ \
		return new y(Me); \
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
	extern "C" DllExport Module * init_module(InspIRCd* Me) \
	{ \
		return new y(Me); \
	}
#endif

#define COMMAND_INIT(c) MODULE_INIT(CommandModule<c>)

#endif
