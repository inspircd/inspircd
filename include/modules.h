/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */


#ifndef __MODULES_H
#define __MODULES_H

/** log levels
 */
enum DebugLevels { DEBUG, VERBOSE, DEFAULT, SPARSE, NONE };

/** Used with OnExtendedMode() method of modules
 */
enum ModeTargetType { MT_CHANNEL, MT_CLIENT, MT_SERVER };

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
	AC_GENERAL_MODE		// a channel mode is being changed
};

/** Used to define a set of behavior bits for a module
 */
enum ModuleFlags {
	VF_STATIC = 1,		// module is static, cannot be /unloadmodule'd
	VF_VENDOR = 2,		// module is a vendor module (came in the original tarball, not 3rd party)
	VF_SERVICEPROVIDER = 4,	// module provides a service to other modules (can be a dependency)
	VF_COMMON = 8		// module needs to be common on all servers in a network to link
};

enum WriteModeFlags {
	WM_AND = 1,
	WM_OR = 2
};

enum TargetTypeFlags {
	TYPE_USER = 1,
	TYPE_CHANNEL,
	TYPE_SERVER,
	TYPE_OTHER
};

#include "dynamic.h"
#include "base.h"
#include "ctables.h"
#include "socket.h"
#include <string>
#include <deque>
#include <sstream>
#include <typeinfo>
#include "timer.h"

class Server;
class ServerConfig;

/** Low level definition of a FileReader classes file cache area
 */
typedef std::deque<std::string> file_cache;
typedef file_cache string_list;

/** Holds a list of users in a channel
 */
typedef std::deque<userrec*> chanuserlist;


// This #define allows us to call a method in all
// loaded modules in a readable simple way, e.g.:
// 'FOREACH_MOD OnConnect(user);'

#define FOREACH_MOD(y,x) if (Config->global_implementation[y] > 0) { \
	for (int _i = 0; _i <= MODCOUNT; _i++) { \
	if (Config->implement_lists[_i][y]) \
		try \
		{ \
			modules[_i]->x ; \
		} \
		catch (ModuleException& modexcept) \
		{ \
			log(DEBUG,"Module exception cought: %s",modexcept.GetReason()); \
		} \
	} \
  }

// This define is similar to the one above but returns a result in MOD_RESULT.
// The first module to return a nonzero result is the value to be accepted,
// and any modules after are ignored.

// *********************************************************************************************

#define FOREACH_RESULT(y,x) { if (Config->global_implementation[y] > 0) { \
			MOD_RESULT = 0; \
			for (int _i = 0; _i <= MODCOUNT; _i++) { \
			if (Config->implement_lists[_i][y]) {\
				try \
				{ \
					int res = modules[_i]->x ; \
					if (res != 0) { \
						MOD_RESULT = res; \
						break; \
					} \
				} \
				catch (ModuleException& modexcept) \
				{ \
					log(DEBUG,"Module exception cought: %s",modexcept.GetReason()); \
				} \
			} \
		} \
	} \
 }
   
// *********************************************************************************************

#define FD_MAGIC_NUMBER -42

// useful macros

#define IS_LOCAL(x) (x->fd > -1)
#define IS_REMOTE(x) (x->fd < 0)
#define IS_MODULE_CREATED(x) (x->fd == FD_MAGIC_NUMBER)

/*extern void createcommand(char* cmd, handlerfunc f, char flags, int minparams, char* source);
extern void server_mode(char **parameters, int pcnt, userrec *user);*/

// class Version holds the version information of a Module, returned
// by Module::GetVersion (thanks RD)

/** Holds a module's Version information
 *  The four members (set by the constructor only) indicate details as to the version number
 *  of a module. A class of type Version is returned by the GetVersion method of the Module class.
 */
class Version : public classbase
{
 public:
	 const int Major, Minor, Revision, Build, Flags;
	 Version(int major, int minor, int revision, int build, int flags);
};

/** Holds /ADMIN data
 *  This class contains the admin details of the local server. It is constructed by class Server,
 *  and has three read-only values, Name, Email and Nick that contain the specified values for the
 *  server where the module is running.
 */
class Admin : public classbase
{
 public:
	 const std::string Name, Email, Nick;
	 Admin(std::string name, std::string email, std::string nick);
};


// Forward-delacare module for ModuleMessage etc
class Module;

// Thanks to Rob (from anope) for the idea of this message passing API
// (its been done before, but this seemed a very neat and tidy way...

/** The ModuleMessage class is the base class of Request and Event
 * This class is used to represent a basic data structure which is passed
 * between modules for safe inter-module communications.
 */
class ModuleMessage : public classbase
{
 public:
        /** This class is pure virtual and must be inherited.
         */
        virtual char* Send() = 0;
	virtual ~ModuleMessage() {};
};

/** The Request class is a unicast message directed at a given module.
 * When this class is properly instantiated it may be sent to a module
 * using the Send() method, which will call the given module's OnRequest
 * method with this class as its parameter.
 */
class Request : public ModuleMessage
{
 protected:
        /** This member holds a pointer to arbitary data set by the emitter of the message
         */
        char* data;
        /** This is a pointer to the sender of the message, which can be used to
         * directly trigger events, or to create a reply.
         */
        Module* source;
        /** The single destination of the Request
         */
        Module* dest;
 public:
        /** Create a new Request
         */
        Request(char* anydata, Module* src, Module* dst);
        /** Fetch the Request data
         */
        char* GetData();
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
        char* Send();
};


/** The Event class is a unicast message directed at all modules.
 * When the class is properly instantiated it may be sent to all modules
 * using the Send() method, which will trigger the OnEvent method in
 * all modules passing the object as its parameter.
 */
class Event : public ModuleMessage
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
        Event(char* anydata, Module* src, std::string eventid);
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
        char* Send();
};

/** Holds an extended mode's details.
 * Used internally by modules.cpp
 */
class ExtMode : public classbase
{
 public:
        char modechar;
        int type;
        bool needsoper;
        int params_when_on;
        int params_when_off;
        bool list;
        ExtMode(char mc, int ty, bool oper, int p_on, int p_off) : modechar(mc), type(ty), needsoper(oper), params_when_on(p_on), params_when_off(p_off), list(false) { };
};


/** This class can be used on its own to represent an exception, or derived to represent a module-specific exception.
 * When a module whishes to abort, e.g. within a constructor, it should throw an exception using ModuleException or
 * a class derived from ModuleException. If a module throws an exception during its constructor, the module will not
 * be loaded. If this happens, the error message returned by ModuleException::GetReason will be displayed to the user
 * attempting to load the module, or dumped to the console if the ircd is currently loading for the first time.
 */
class ModuleException
{
 private:
	/** Holds the error message to be displayed
	 */
	std::string err;
 public:
	/** Default constructor, just uses the error mesage 'Module threw an exception'.
	 */
	ModuleException() : err("Module threw an exception") {}
	/** This constructor can be used to specify an error message before throwing.
	 */
	ModuleException(std::string message) : err(message) {}
	/** This destructor solves world hunger, cancels the world debt, and causes the world to end.
	 * Actually no, it does nothing. Never mind.
	 */
	virtual ~ModuleException() {};
	/** Returns the reason for the exception.
	 * The module should probably put something informative here as the user will see this upon failure.
	 */
	virtual char *GetReason()
	{
		return (char*)err.c_str();
	}
};

/** Priority types which can be returned from Module::Prioritize()
 */
enum Priority { PRIORITY_FIRST, PRIORITY_DONTCARE, PRIORITY_LAST, PRIORITY_BEFORE, PRIORITY_AFTER };

/** Implementation-specific flags which may be set in Module::Implements()
 */
enum Implementation {	I_OnUserConnect, I_OnUserQuit, I_OnUserDisconnect, I_OnUserJoin, I_OnUserPart, I_OnRehash, I_OnServerRaw, 
			I_OnExtendedMode, I_OnUserPreJoin, I_OnUserPreKick, I_OnUserKick, I_OnOper, I_OnInfo, I_OnWhois, I_OnUserPreInvite,
			I_OnUserInvite, I_OnUserPreMessage, I_OnUserPreNotice, I_OnUserPreNick, I_OnUserMessage, I_OnUserNotice, I_OnMode,
			I_OnGetServerDescription, I_OnSyncUser, I_OnSyncChannel, I_OnSyncChannelMetaData, I_OnSyncUserMetaData,
			I_OnDecodeMetaData, I_ProtoSendMode, I_ProtoSendMetaData, I_OnWallops, I_OnChangeHost, I_OnChangeName, I_OnAddGLine,
			I_OnAddZLine, I_OnAddQLine, I_OnAddKLine, I_OnAddELine, I_OnDelGLine, I_OnDelZLine, I_OnDelKLine, I_OnDelELine, I_OnDelQLine,
			I_OnCleanup, I_OnUserPostNick, I_OnAccessCheck, I_On005Numeric, I_OnKill, I_OnRemoteKill, I_OnLoadModule, I_OnUnloadModule,
			I_OnBackgroundTimer, I_OnSendList, I_OnPreCommand, I_OnCheckReady, I_OnUserRrgister, I_OnRawMode, I_OnCheckInvite,
			I_OnCheckKey, I_OnCheckLimit, I_OnCheckBan, I_OnStats, I_OnChangeLocalUserHost, I_OnChangeLocalUserGecos, I_OnLocalTopicChange,
			I_OnPostLocalTopicChange, I_OnEvent, I_OnRequest, I_OnOperCompre, I_OnGlobalOper, I_OnGlobalConnect, I_OnAddBan, I_OnDelBan,
			I_OnRawSocketAccept, I_OnRawSocketClose, I_OnRawSocketWrite, I_OnRawSocketRead, I_OnChangeLocalUserGECOS, I_OnUserRegister,
			I_OnOperCompare, I_OnChannelDelete, I_OnPostOper, I_OnSyncOtherMetaData, I_OnSetAway, I_OnCancelAway };

/** Base class for all InspIRCd modules
 *  This class is the base class for InspIRCd modules. All modules must inherit from this class,
 *  its methods will be called when irc server events occur. class inherited from module must be
 *  instantiated by the ModuleFactory class (see relevent section) for the plugin to be initialised.
 */
class Module : public classbase
{
 public:

	/** Default constructor
	 * Creates a module class.
	 * @param Me An instance of the Server class which can be saved for future use
	 * \exception ModuleException Throwing this class, or any class derived from ModuleException, causes loading of the module to abort.
	 */
	Module(Server* Me);

	/** Default destructor
	 * destroys a module class
	 */
	virtual ~Module();

	/** Returns the version number of a Module.
	 * The method should return a Version object with its version information assigned via
	 * Version::Version
	 */
	virtual Version GetVersion();

	/** The Implements function specifies which methods a module should receive events for.
	 * The char* parameter passed to this function contains a set of true or false values
	 * (1 or 0) which indicate wether each function is implemented. You must use the Iimplementation
	 * enum (documented elsewhere on this page) to mark functions as active. For example, to
	 * receive events for OnUserJoin():
	 *
	 * Implements[I_OnUserJoin] = 1;
	 *
	 * @param The implement list
	 */
	virtual void Implements(char* Implements);

	/** Used to set the 'priority' of a module (e.g. when it is called in relation to other modules.
	 * Some modules prefer to be called before other modules, due to their design. For example, a
	 * module which is expected to operate on complete information would expect to be placed last, so
	 * that any other modules which wish to adjust that information would execute before it, to be sure
	 * its information is correct. You can change your module's priority by returning one of:
	 *
	 * PRIORITY_FIRST - To place your module first in the list
	 * 
	 * PRIORITY_LAST - To place your module last in the list
	 *
	 * PRIORITY_DONTCARE - To leave your module as it is (this is the default value, if you do not implement this function)
	 *
	 * The result of Server::PriorityBefore() - To move your module before another named module
	 *
	 * The result of Server::PriorityLast() - To move your module after another named module
	 *
	 * For a good working example of this method call, please see src/modules/m_spanningtree.cpp
	 * and src/modules/m_hostchange.so which make use of it. It is highly recommended that unless
	 * your module has a real need to reorder its priority, it should not implement this function,
	 * as many modules changing their priorities can make the system redundant.
	 */
	virtual Priority Prioritize();

	/** Called when a user connects.
	 * The details of the connecting user are available to you in the parameter userrec *user
	 * @param user The user who is connecting
	 */
	virtual void OnUserConnect(userrec* user);

	/** Called when a user quits.
	 * The details of the exiting user are available to you in the parameter userrec *user
	 * This event is only called when the user is fully registered when they quit. To catch
	 * raw disconnections, use the OnUserDisconnect method.
	 * @param user The user who is quitting
	 * @param message The user's quit message
	 */
	virtual void OnUserQuit(userrec* user, std::string message);

	/** Called whenever a user's socket is closed.
	 * The details of the exiting user are available to you in the parameter userrec *user
	 * This event is called for all users, registered or not, as a cleanup method for modules
	 * which might assign resources to user, such as dns lookups, objects and sockets.
	 * @param user The user who is disconnecting
	 */
	virtual void OnUserDisconnect(userrec* user);

	/** Called whenever a channel is deleted, either by QUIT, KICK or PART.
	 * @param chan The channel being deleted
	 */
	virtual void OnChannelDelete(chanrec* chan);

	/** Called when a user joins a channel.
	 * The details of the joining user are available to you in the parameter userrec *user,
	 * and the details of the channel they have joined is available in the variable chanrec *channel
	 * @param user The user who is joining
	 * @param channel The channel being joined
	 */
	virtual void OnUserJoin(userrec* user, chanrec* channel);

	/** Called when a user parts a channel.
	 * The details of the leaving user are available to you in the parameter userrec *user,
	 * and the details of the channel they have left is available in the variable chanrec *channel
	 * @param user The user who is parting
	 * @param channel The channel being parted
	 * @param partmessage The part message, or an empty string
	 */
	virtual void OnUserPart(userrec* user, chanrec* channel, std::string partmessage);

	/** Called on rehash.
	 * This method is called prior to a /REHASH or when a SIGHUP is received from the operating
	 * system. You should use it to reload any files so that your module keeps in step with the
	 * rest of the application. If a parameter is given, the core has done nothing. The module
	 * receiving the event can decide if this parameter has any relevence to it.
	 * @param parameter The (optional) parameter given to REHASH from the user.
	 */
 	virtual void OnRehash(std::string parameter);

	/** Called when a raw command is transmitted or received.
	 * This method is the lowest level of handler available to a module. It will be called with raw
	 * data which is passing through a connected socket. If you wish, you may munge this data by changing
	 * the string parameter "raw". If you do this, after your function exits it will immediately be
	 * cut down to 510 characters plus a carriage return and linefeed. For INBOUND messages only (where
	 * inbound is set to true) the value of user will be the userrec of the connection sending the
	 * data. This is not possible for outbound data because the data may be being routed to multiple targets.
	 * @param raw The raw string in RFC1459 format
	 * @param inbound A flag to indicate wether the data is coming into the daemon or going out to the user
	 * @param user The user record sending the text, when inbound == true.
	 */
 	virtual void OnServerRaw(std::string &raw, bool inbound, userrec* user);

	/** Called whenever an extended mode is to be processed.
	 * The type parameter is MT_SERVER, MT_CLIENT or MT_CHANNEL, dependent on where the mode is being
	 * changed. mode_on is set when the mode is being set, in which case params contains a list of
	 * parameters for the mode as strings. If mode_on is false, the mode is being removed, and parameters
	 * may contain the parameters for the mode, dependent on wether they were defined when a mode handler
 	 * was set up with Server::AddExtendedMode
 	 * If the mode is a channel mode, target is a chanrec*, and if it is a user mode, target is a userrec*.
 	 * You must cast this value yourself to make use of it.
	 * @param user The user issuing the mode
	 * @param target The user or channel having the mode set on them
	 * @param modechar The mode character being set
	 * @param type The type of mode (user or channel) being set
	 * @param mode_on True if the mode is being set, false if it is being unset
	 * @param params A list of parameters for any channel mode (currently supports either 0 or 1 parameters)
	 */
 	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params);
 	
	/** Called whenever a user is about to join a channel, before any processing is done.
	 * Returning a value of 1 from this function stops the process immediately, causing no
	 * output to be sent to the user by the core. If you do this you must produce your own numerics,
	 * notices etc. This is useful for modules which may want to mimic +b, +k, +l etc. Returning -1 from
	 * this function forces the join to be allowed, bypassing restrictions such as banlists, invite, keys etc.
	 *
	 * IMPORTANT NOTE!
	 *
	 * If the user joins a NEW channel which does not exist yet, OnUserPreJoin will be called BEFORE the channel
	 * record is created. This will cause chanrec* chan to be NULL. There is very little you can do in form of
	 * processing on the actual channel record at this point, however the channel NAME will still be passed in
	 * char* cname, so that you could for example implement a channel blacklist or whitelist, etc.
	 * @param user The user joining the channel
	 * @param cname The channel name being joined
	 * @return 1 To prevent the join, 0 to allow it.
	 */
	virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname);
	
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
	virtual int OnUserPreKick(userrec* source, userrec* user, chanrec* chan, std::string reason);

	/** Called whenever a user is kicked.
	 * If this method is called, the kick is already underway and cannot be prevented, so
	 * to prevent a kick, please use Module::OnUserPreKick instead of this method.
	 * @param source The user issuing the kick
	 * @param user The user being kicked
	 * @param chan The channel the user is being kicked from
	 * @param reason The kick reason
	 */
	virtual void OnUserKick(userrec* source, userrec* user, chanrec* chan, std::string reason);

	/** Called whenever a user opers locally.
	 * The userrec will contain the oper mode 'o' as this function is called after any modifications
	 * are made to the user's structure by the core.
	 * @param user The user who is opering up
	 * @param opertype The opers type name
	 */
	virtual void OnOper(userrec* user, std::string opertype);

	/** Called after a user opers locally.
	 * This is identical to Module::OnOper(), except it is called after OnOper so that other modules
	 * can be gauranteed to already have processed the oper-up, for example m_spanningtree has sent
	 * out the OPERTYPE, etc.
	 * @param user The user who is opering up
	 * @param opertype The opers type name
	 */
	virtual void OnPostOper(userrec* user, std::string opertype);
	
	/** Called whenever a user types /INFO.
	 * The userrec will contain the information of the user who typed the command. Modules may use this
	 * method to output their own credits in /INFO (which is the ircd's version of an about box).
	 * It is purposefully not possible to modify any info that has already been output, or halt the list.
	 * You must write a 371 numeric to the user, containing your info in the following format:
	 *
	 * &lt;nick&gt; :information here
	 *
	 * @param user The user issuing /INFO
	 */
	virtual void OnInfo(userrec* user);
	
	/** Called whenever a /WHOIS is performed on a local user.
	 * The source parameter contains the details of the user who issued the WHOIS command, and
	 * the dest parameter contains the information of the user they are whoising.
	 * @param source The user issuing the WHOIS command
	 * @param dest The user who is being WHOISed
	 */
	virtual void OnWhois(userrec* source, userrec* dest);
	
	/** Called whenever a user is about to invite another user into a channel, before any processing is done.
	 * Returning 1 from this function stops the process immediately, causing no
	 * output to be sent to the user by the core. If you do this you must produce your own numerics,
	 * notices etc. This is useful for modules which may want to filter invites to channels.
	 * @param source The user who is issuing the INVITE
	 * @param dest The user being invited
	 * @param channel The channel the user is being invited to
	 * @return 1 to deny the invite, 0 to allow
	 */
	virtual int OnUserPreInvite(userrec* source,userrec* dest,chanrec* channel);
	
	/** Called after a user has been successfully invited to a channel.
	 * You cannot prevent the invite from occuring using this function, to do that,
	 * use OnUserPreInvite instead.
	 * @param source The user who is issuing the INVITE
	 * @param dest The user being invited
	 * @param channel The channel the user is being invited to
	 */
	virtual void OnUserInvite(userrec* source,userrec* dest,chanrec* channel);
	
	/** Called whenever a user is about to PRIVMSG A user or a channel, before any processing is done.
	 * Returning any nonzero value from this function stops the process immediately, causing no
	 * output to be sent to the user by the core. If you do this you must produce your own numerics,
	 * notices etc. This is useful for modules which may want to filter or redirect messages.
	 * target_type can be one of TYPE_USER or TYPE_CHANNEL. If the target_type value is a user,
	 * you must cast dest to a userrec* otherwise you must cast it to a chanrec*, this is the details
	 * of where the message is destined to be sent.
	 * @param user The user sending the message
	 * @param dest The target of the message (chanrec* or userrec*)
	 * @param target_type The type of target (TYPE_USER or TYPE_CHANNEL)
	 * @param text Changeable text being sent by the user
	 * @param status The status being used, e.g. PRIVMSG @#chan has status== '@', 0 to send to everyone.
	 * @return 1 to deny the NOTICE, 0 to allow it
	 */
	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text,char status);

	/** Called whenever a user is about to NOTICE A user or a channel, before any processing is done.
	 * Returning any nonzero value from this function stops the process immediately, causing no
	 * output to be sent to the user by the core. If you do this you must produce your own numerics,
	 * notices etc. This is useful for modules which may want to filter or redirect messages.
	 * target_type can be one of TYPE_USER or TYPE_CHANNEL. If the target_type value is a user,
	 * you must cast dest to a userrec* otherwise you must cast it to a chanrec*, this is the details
	 * of where the message is destined to be sent.
	 * You may alter the message text as you wish before relinquishing control to the next module
	 * in the chain, and if no other modules block the text this altered form of the text will be sent out
	 * to the user and possibly to other servers.
	 * @param user The user sending the message
	 * @param dest The target of the message (chanrec* or userrec*)
	 * @param target_type The type of target (TYPE_USER or TYPE_CHANNEL)
	 * @param text Changeable text being sent by the user
	 * @param status The status being used, e.g. PRIVMSG @#chan has status== '@', 0 to send to everyone.
	 * @return 1 to deny the NOTICE, 0 to allow it
	 */
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text,char status);
	
	/** Called before any nickchange, local or remote. This can be used to implement Q-lines etc.
	 * Please note that although you can see remote nickchanges through this function, you should
	 * NOT make any changes to the userrec if the user is a remote user as this may cause a desnyc.
	 * check user->server before taking any action (including returning nonzero from the method).
	 * If your method returns nonzero, the nickchange is silently forbidden, and it is down to your
	 * module to generate some meaninful output.
	 * @param user The username changing their nick
	 * @param newnick Their new nickname
	 * @return 1 to deny the change, 0 to allow
	 */
	virtual int OnUserPreNick(userrec* user, std::string newnick);

	/** Called after any PRIVMSG sent from a user.
	 * The dest variable contains a userrec* if target_type is TYPE_USER and a chanrec*
	 * if target_type is TYPE_CHANNEL.
	 * @param user The user sending the message
	 * @param dest The target of the message
	 * @param target_type The type of target (TYPE_USER or TYPE_CHANNEL)
	 * @param text the text being sent by the user
	 * @param status The status being used, e.g. PRIVMSG @#chan has status== '@', 0 to send to everyone.
	 */
	virtual void OnUserMessage(userrec* user, void* dest, int target_type, std::string text, char status);

        /** Called after any NOTICE sent from a user.
	 * The dest variable contains a userrec* if target_type is TYPE_USER and a chanrec*
	 * if target_type is TYPE_CHANNEL.
	 * @param user The user sending the message
	 * @param dest The target of the message
	 * @param target_type The type of target (TYPE_USER or TYPE_CHANNEL)
	 * @param text the text being sent by the user
	 * @param status The status being used, e.g. NOTICE @#chan has status== '@', 0 to send to everyone.
	 */
	virtual void OnUserNotice(userrec* user, void* dest, int target_type, std::string text, char status);

	/** Called after every MODE command sent from a user
	 * The dest variable contains a userrec* if target_type is TYPE_USER and a chanrec*
	 * if target_type is TYPE_CHANNEL. The text variable contains the remainder of the
	 * mode string after the target, e.g. "+wsi" or "+ooo nick1 nick2 nick3".
	 * @param user The user sending the MODEs
	 * @param dest The target of the modes (userrec* or chanrec*)
	 * @param target_type The type of target (TYPE_USER or TYPE_CHANNEL)
	 * @param text The actual modes and their parameters if any
	 */
	virtual void OnMode(userrec* user, void* dest, int target_type, std::string text);

	/** Allows modules to alter or create server descriptions
	 * Whenever a module requires a server description, for example for display in
	 * WHOIS, this function is called in all modules. You may change or define the
	 * description given in std::string &description. If you do, this description
	 * will be shown in the WHOIS fields.
	 * @param servername The servername being searched for
	 * @param description Alterable server description for this server
	 */
	virtual void OnGetServerDescription(std::string servername,std::string &description);

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
	virtual void OnSyncUser(userrec* user, Module* proto, void* opaque);

	/** Allows modules to synchronize data which relates to channels during a netburst.
	 * When this function is called, it will be called from the module which implements
	 * the linking protocol. This currently is m_spanningtree.so. A pointer to this module
	 * is given in Module* proto, so that you may call its methods such as ProtoSendMode
	 * (see below). This function will be called for every user visible on your side
	 * of the burst, allowing you to for example set modes, etc. Do not use this call to
	 * synchronize data which you have stored using class Extensible -- There is a specialist
	 * function OnSyncUserMetaData and OnSyncChannelMetaData for this!
	 *
	 * For a good example of how to use this function, please see src/modules/m_chanprotect.cpp
	 *
	 * @param chan The channel being syncronized
	 * @param proto A pointer to the module handling network protocol
	 * @param opaque An opaque pointer set by the protocol module, should not be modified!
	 */
	virtual void OnSyncChannel(chanrec* chan, Module* proto, void* opaque);

	/* Allows modules to syncronize metadata related to channels over the network during a netburst.
	 * Whenever the linking module wants to send out data, but doesnt know what the data
	 * represents (e.g. it is Extensible metadata, added to a userrec or chanrec by a module) then
	 * this method is called.You should use the ProtoSendMetaData function after you've
	 * correctly decided how the data should be represented, to send the metadata on its way if it belongs
	 * to your module. For a good example of how to use this method, see src/modules/m_swhois.cpp.
	 * @param chan The channel whos metadata is being syncronized
	 * @param proto A pointer to the module handling network protocol
	 * @param opaque An opaque pointer set by the protocol module, should not be modified!
	 * @param extname The extensions name which is being searched for
	 */
	virtual void OnSyncChannelMetaData(chanrec* chan, Module* proto,void* opaque, std::string extname);

	/* Allows modules to syncronize metadata related to users over the network during a netburst.
	 * Whenever the linking module wants to send out data, but doesnt know what the data
	 * represents (e.g. it is Extensible metadata, added to a userrec or chanrec by a module) then
	 * this method is called. You should use the ProtoSendMetaData function after you've
	 * correctly decided how the data should be represented, to send the metadata on its way if
	 * if it belongs to your module.
	 * @param user The user whos metadata is being syncronized
	 * @param proto A pointer to the module handling network protocol
	 * @param opaque An opaque pointer set by the protocol module, should not be modified!
	 * @param extname The extensions name which is being searched for
	 */
	virtual void OnSyncUserMetaData(userrec* user, Module* proto,void* opaque, std::string extname);

	/* Allows modules to syncronize metadata not related to users or channels, over the network during a netburst.
	 * Whenever the linking module wants to send out data, but doesnt know what the data
	 * represents (e.g. it is Extensible metadata, added to a userrec or chanrec by a module) then
	 * this method is called. You should use the ProtoSendMetaData function after you've
	 * correctly decided how the data should be represented, to send the metadata on its way if
	 * if it belongs to your module.
	 * @param proto A pointer to the module handling network protocol
	 * @param opaque An opaque pointer set by the protocol module, should not be modified!
	 */
	virtual void OnSyncOtherMetaData(Module* proto, void* opaque);

	/** Allows module data, sent via ProtoSendMetaData, to be decoded again by a receiving module.
	 * Please see src/modules/m_swhois.cpp for a working example of how to use this method call.
	 * @param target_type The type of item to decode data for, TYPE_USER or TYPE_CHANNEL
	 * @param target The chanrec* or userrec* that data should be added to
	 * @param extname The extension name which is being sent
	 * @param extdata The extension data, encoded at the other end by an identical module through OnSyncChannelMetaData or OnSyncUserMetaData
	 */
	virtual void OnDecodeMetaData(int target_type, void* target, std::string extname, std::string extdata);

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
	 * @param target The chanrec* or userrec* that modes should be sent for
	 * @param modeline The modes and parameters to be sent
	 */
	virtual void ProtoSendMode(void* opaque, int target_type, void* target, std::string modeline);

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
	 * @param target The chanrec* or userrec* that metadata should be sent for
	 * @param extname The extension name to send metadata for
	 * @param extdata Encoded data for this extension name, which will be encoded at the oppsite end by an identical module using OnDecodeMetaData
	 */
	virtual void ProtoSendMetaData(void* opaque, int target_type, void* target, std::string extname, std::string extdata);
	
	/** Called after every WALLOPS command.
	 * @param user The user sending the WALLOPS
	 * @param text The content of the WALLOPS message
	 */
	virtual void OnWallops(userrec* user, std::string text);

	/** Called whenever a user's hostname is changed.
	 * This event triggers after the host has been set.
	 * @param user The user whos host is being changed
	 * @param newhost The new hostname being set
	 */
	virtual void OnChangeHost(userrec* user, std::string newhost);

	/** Called whenever a user's GECOS (realname) is changed.
	 * This event triggers after the name has been set.
	 * @param user The user who's GECOS is being changed
	 * @param gecos The new GECOS being set on the user
	 */
	virtual void OnChangeName(userrec* user, std::string gecos);

	/** Called whenever a gline is added by a local user.
	 * This method is triggered after the line is added.
	 * @param duration The duration of the line in seconds
	 * @param source The sender of the line
	 * @param reason The reason text to be displayed
	 * @param hostmask The hostmask to add
	 */
	virtual void OnAddGLine(long duration, userrec* source, std::string reason, std::string hostmask);

	/** Called whenever a zline is added by a local user.
	 * This method is triggered after the line is added.
         * @param duration The duration of the line in seconds
         * @param source The sender of the line
         * @param reason The reason text to be displayed
         * @param ipmask The hostmask to add
	 */
	virtual void OnAddZLine(long duration, userrec* source, std::string reason, std::string ipmask);

	/** Called whenever a kline is added by a local user.
	 * This method is triggered after the line is added.
         * @param duration The duration of the line in seconds
         * @param source The sender of the line
         * @param reason The reason text to be displayed
         * @param hostmask The hostmask to add
	 */
	virtual void OnAddKLine(long duration, userrec* source, std::string reason, std::string hostmask);

	/** Called whenever a qline is added by a local user.
	 * This method is triggered after the line is added.
         * @param duration The duration of the line in seconds
         * @param source The sender of the line
         * @param reason The reason text to be displayed
         * @param nickmask The hostmask to add
	 */
	virtual void OnAddQLine(long duration, userrec* source, std::string reason, std::string nickmask);

	/** Called whenever a eline is added by a local user.
	 * This method is triggered after the line is added.
         * @param duration The duration of the line in seconds
         * @param source The sender of the line
         * @param reason The reason text to be displayed
         * @param hostmask The hostmask to add
	 */
	virtual void OnAddELine(long duration, userrec* source, std::string reason, std::string hostmask);

	/** Called whenever a gline is deleted.
	 * This method is triggered after the line is deleted.
         * @param source The user removing the line
         * @param hostmask The hostmask to delete
	 */
	virtual void OnDelGLine(userrec* source, std::string hostmask);

	/** Called whenever a zline is deleted.
	 * This method is triggered after the line is deleted.
         * @param source The user removing the line
         * @param hostmask The hostmask to delete
	 */
	virtual void OnDelZLine(userrec* source, std::string ipmask);

	/** Called whenever a kline is deleted.
	 * This method is triggered after the line is deleted.
         * @param source The user removing the line
         * @param hostmask The hostmask to delete
	 */
	virtual void OnDelKLine(userrec* source, std::string hostmask);
	
	/** Called whenever a qline is deleted.
	 * This method is triggered after the line is deleted.
         * @param source The user removing the line
         * @param hostmask The hostmask to delete
	 */
	virtual void OnDelQLine(userrec* source, std::string nickmask);

	/** Called whenever a eline is deleted.
	 * This method is triggered after the line is deleted.
         * @param source The user removing the line
         * @param hostmask The hostmask to delete
	 */
	virtual void OnDelELine(userrec* source, std::string hostmask);

	/** Called before your module is unloaded to clean up Extensibles.
	 * This method is called once for every user and channel on the network,
	 * so that when your module unloads it may clear up any remaining data
	 * in the form of Extensibles added using Extensible::Extend().
	 * If the target_type variable is TYPE_USER, then void* item refers to
	 * a userrec*, otherwise it refers to a chanrec*.
	 * @param target_type The type of item being cleaned
	 * @param item A pointer to the item's class
	 */
	virtual void OnCleanup(int target_type, void* item);

	/** Called after any nickchange, local or remote. This can be used to track users after nickchanges
	 * have been applied. Please note that although you can see remote nickchanges through this function, you should
         * NOT make any changes to the userrec if the user is a remote user as this may cause a desnyc.
         * check user->server before taking any action (including returning nonzero from the method).
	 * Because this method is called after the nickchange is taken place, no return values are possible
	 * to indicate forbidding of the nick change. Use OnUserPreNick for this.
	 * @param user The user changing their nick
	 * @param oldnick The old nickname of the user before the nickchange
         */
	virtual void OnUserPostNick(userrec* user, std::string oldnick);

	/** Called before an action which requires a channel privilage check.
	 * This function is called before many functions which check a users status on a channel, for example
	 * before opping a user, deopping a user, kicking a user, etc.
	 * There are several values for access_type which indicate for what reason access is being checked.
	 * These are:<br><br>
	 * AC_KICK (0) - A user is being kicked<br>
	 * AC_DEOP (1) - a user is being deopped<br>
	 * AC_OP (2) - a user is being opped<br>
	 * AC_VOICE (3) - a user is being voiced<br>
	 * AC_DEVOICE (4) - a user is being devoiced<br>
	 * AC_HALFOP (5) - a user is being halfopped<br>
	 * AC_DEHALFOP (6) - a user is being dehalfopped<br>
	 * AC_INVITE (7) - a user is being invited<br>
	 * AC_GENERAL_MODE (8) - a user channel mode is being changed<br><br>
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
	virtual int OnAccessCheck(userrec* source,userrec* dest,chanrec* channel,int access_type);

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
	virtual int OnKill(userrec* source, userrec* dest, std::string reason);

	/** Called when an oper wants to disconnect a remote user via KILL
	 * @param source The user sending the KILL
	 * @param dest The user being killed
	 * @param reason The kill reason
	 */
	virtual void OnRemoteKill(userrec* source, userrec* dest, std::string reason);

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
	virtual void OnLoadModule(Module* mod,std::string name);

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
	virtual void OnUnloadModule(Module* mod,std::string name);

	/** Called once every five seconds for background processing.
	 * This timer can be used to control timed features. Its period is not accurate
	 * enough to be used as a clock, but it is gauranteed to be called at least once in
	 * any five second period, directly from the main loop of the server.
	 * @param curtime The current timer derived from time(2)
	 */
	virtual void OnBackgroundTimer(time_t curtime);

	/** Called whenever a list is needed for a listmode.
	 * For example, when a /MODE #channel +b (without any other parameters) is called,
	 * if a module was handling +b this function would be called. The function can then
	 * output any lists it wishes to. Please note that all modules will see all mode
	 * characters to provide the ability to extend each other, so please only output
	 * a list if the mode character given matches the one(s) you want to handle.
	 * @param user The user requesting the list
	 * @param channel The channel the list is for
	 * @param mode The listmode which a list is being requested on
	 */
	virtual void OnSendList(userrec* user, chanrec* channel, char mode);

	/** Called whenever any command is about to be executed.
	 * This event occurs for all registered commands, wether they are registered in the core,
	 * or another module, but it will not occur for invalid commands (e.g. ones which do not
	 * exist within the command table). By returning 1 from this method you may prevent the
	 * command being executed. If you do this, no output is created by the core, and it is
	 * down to your module to produce any output neccessary.
	 * Note that unless you return 1, you should not destroy any structures (e.g. by using
	 * Server::QuitUser) otherwise when the command's handler function executes after your
	 * method returns, it will be passed an invalid pointer to the user object and crash!)
	 * @param command The command being executed
	 * @param parameters An array of array of characters containing the parameters for the command
	 * @param pcnt The nuimber of parameters passed to the command
	 * @param user the user issuing the command
	 * @param validated True if the command has passed all checks, e.g. it is recognised, has enough parameters, the user has permission to execute it, etc.
	 * @return 1 to block the command, 0 to allow
	 */
	virtual int OnPreCommand(std::string command, char **parameters, int pcnt, userrec *user, bool validated);

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
	virtual bool OnCheckReady(userrec* user);

	/** Called whenever a user is about to register their connection (e.g. before the user
	 * is sent the MOTD etc). Modules can use this method if they are performing a function
	 * which must be done before the actual connection is completed (e.g. ident lookups,
	 * dnsbl lookups, etc).
	 * Note that you should NOT delete the user record here by causing a disconnection!
	 * Use OnUserConnect for that instead.
	 * @param user The user registering
	 */
	virtual void OnUserRegister(userrec* user);

	/** Called whenever a mode character is processed.
	 * Return 1 from this function to block the mode character from being processed entirely,
	 * so that you may perform your own code instead. Note that this method allows you to override
	 * modes defined by other modes, but this is NOT RECOMMENDED!
	 * @param user The user who is sending the mode
	 * @param chan The channel the mode is being sent to
	 * @param mode The mode character being set
	 * @param param The parameter for the mode or an empty string
	 * @param adding true of the mode is being added, false if it is being removed
	 * @param pcnt The parameter count for the mode (0 or 1)
	 * @return 1 to deny the mode, 0 to allow
	 */
	virtual int OnRawMode(userrec* user, chanrec* chan, char mode, std::string param, bool adding, int pcnt);

	/** Called whenever a user joins a channel, to determine if invite checks should go ahead or not.
	 * This method will always be called for each join, wether or not the channel is actually +i, and
	 * determines the outcome of an if statement around the whole section of invite checking code.
	 * return 1 to explicitly allow the join to go ahead or 0 to ignore the event.
	 * @param user The user joining the channel
	 * @param chan The channel being joined
	 * @return 1 to explicitly allow the join, 0 to proceed as normal
	 */
	virtual int OnCheckInvite(userrec* user, chanrec* chan);

        /** Called whenever a user joins a channel, to determine if key checks should go ahead or not.
         * This method will always be called for each join, wether or not the channel is actually +k, and
         * determines the outcome of an if statement around the whole section of key checking code.
	 * if the user specified no key, the keygiven string will be a valid but empty value.
         * return 1 to explicitly allow the join to go ahead or 0 to ignore the event.
	 * @param user The user joining the channel
	 * @param chan The channel being joined
	 * @return 1 to explicitly allow the join, 0 to proceed as normal
         */
	virtual int OnCheckKey(userrec* user, chanrec* chan, std::string keygiven);

        /** Called whenever a user joins a channel, to determine if channel limit checks should go ahead or not.
         * This method will always be called for each join, wether or not the channel is actually +l, and
         * determines the outcome of an if statement around the whole section of channel limit checking code.
         * return 1 to explicitly allow the join to go ahead or 0 to ignore the event.
	 * @param user The user joining the channel
	 * @param chan The channel being joined
	 * @return 1 to explicitly allow the join, 0 to proceed as normal
         */
	virtual int OnCheckLimit(userrec* user, chanrec* chan);

        /** Called whenever a user joins a channel, to determine if banlist checks should go ahead or not.
         * This method will always be called for each join, wether or not the user actually matches a channel ban, and
         * determines the outcome of an if statement around the whole section of ban checking code.
         * return 1 to explicitly allow the join to go ahead or 0 to ignore the event.
	 * @param user The user joining the channel
	 * @param chan The channel being joined
	 * @return 1 to explicitly allow the join, 0 to proceed as normal
         */
	virtual int OnCheckBan(userrec* user, chanrec* chan);

	/** Called on all /STATS commands
	 * This method is triggered for all /STATS use, including stats symbols handled by the core.
	 * @param symbol the symbol provided to /STATS
	 * @user the user issuing the /STATS command
	 * @return 1 to block the /STATS from being processed by the core, 0 to allow it
	 */
	virtual int OnStats(char symbol, userrec* user);

	/** Called whenever a change of a local users displayed host is attempted.
	 * Return 1 to deny the host change, or 0 to allow it.
	 * @param user The user whos host will be changed
	 * @param newhost The new hostname
	 * @return 1 to deny the host change, 0 to allow
	 */
	virtual int OnChangeLocalUserHost(userrec* user, std::string newhost);

	/** Called whenever a change of a local users GECOS (fullname field) is attempted.
	 * return 1 to deny the name change, or 0 to allow it.
	 * @param user The user whos GECOS will be changed
	 * @param newhost The new GECOS
	 * @return 1 to deny the GECOS change, 0 to allow
	 */
	virtual int OnChangeLocalUserGECOS(userrec* user, std::string newhost); 

	/** Called whenever a topic is changed by a local user.
	 * Return 1 to deny the topic change, or 0 to allow it.
	 * @param user The user changing the topic
	 * @param chan The channels who's topic is being changed
	 * @param topic The actual topic text
	 * @param 1 to block the topic change, 0 to allow
	 */
	virtual int OnLocalTopicChange(userrec* user, chanrec* chan, std::string topic);

	/** Called whenever a local topic has been changed.
	 * To block topic changes you must use OnLocalTopicChange instead.
	 * @param user The user changing the topic
	 * @param chan The channels who's topic is being changed
	 * @param topic The actual topic text
	 */
	virtual void OnPostLocalTopicChange(userrec* user, chanrec* chan, std::string topic);

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
	virtual char* OnRequest(Request* request);

	/** Called whenever an oper password is to be compared to what a user has input.
	 * The password field (from the config file) is in 'password' and is to be compared against
	 * 'input'. This method allows for encryption of oper passwords and much more besides.
	 * You should return a nonzero value if you want to allow the comparison or zero if you wish
	 * to do nothing.
	 * @param password The oper's password
	 * @param input The password entered
	 * @return 1 to match the passwords, 0 to do nothing
	 */
	virtual int OnOperCompare(std::string password, std::string input);

	/** Called whenever a user is given usermode +o, anywhere on the network.
	 * You cannot override this and prevent it from happening as it is already happened and
	 * such a task must be performed by another server. You can however bounce modes by sending
	 * servermodes out to reverse mode changes.
	 * @param user The user who is opering
	 */
	virtual void OnGlobalOper(userrec* user);

	/**  Called whenever a user connects, anywhere on the network.
	 * This event is informational only. You should not change any user information in this
	 * event. To do so, use the OnUserConnect method to change the state of local users.
	 * @param user The user who is connecting
	 */
	virtual void OnGlobalConnect(userrec* user);

	/** Called whenever a ban is added to a channel's list.
	 * Return a non-zero value to 'eat' the mode change and prevent the ban from being added.
	 * @param source The user adding the ban
	 * @param channel The channel the ban is being added to
	 * @param banmask The ban mask being added
	 * @return 1 to block the ban, 0 to continue as normal
	 */
	virtual int OnAddBan(userrec* source, chanrec* channel,std::string banmask);

	/** Called whenever a ban is removed from a channel's list.
	 * Return a non-zero value to 'eat' the mode change and prevent the ban from being removed.
	 * @param source The user deleting the ban
	 * @param channel The channel the ban is being deleted from
	 * @param banmask The ban mask being deleted
	 * @return 1 to block the unban, 0 to continue as normal
	 */
	virtual int OnDelBan(userrec* source, chanrec* channel,std::string banmask);

	/** Called immediately after any  connection is accepted. This is intended for raw socket
	 * processing (e.g. modules which wrap the tcp connection within another library) and provides
	 * no information relating to a user record as the connection has not been assigned yet.
	 * There are no return values from this call as all modules get an opportunity if required to
	 * process the connection.
	 * @param fd The file descriptor returned from accept()
	 * @param ip The IP address of the connecting user
	 * @param localport The local port number the user connected to
	 */
	virtual void OnRawSocketAccept(int fd, std::string ip, int localport);

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
	virtual int OnRawSocketWrite(int fd, char* buffer, int count);

	/** Called immediately before any socket is closed. When this event is called, shutdown()
	 * has not yet been called on the socket.
	 * @param fd The file descriptor of the socket prior to close()
	 */
	virtual void OnRawSocketClose(int fd);

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

	virtual void OnSetAway(userrec* user);

	virtual void OnCancelAway(userrec* user);
};


/** Allows server output and query functions
 * This class contains methods which allow a module to query the state of the irc server, and produce
 * output to users and other servers. All modules should instantiate at least one copy of this class,
 * and use its member functions to perform their tasks.
 */
class Server : public classbase
{
 public:
	/** Default constructor.
	 * Creates a Server object.
	 */
	Server();

	/** Default destructor.
	 * Destroys a Server object.
	 */
	virtual ~Server();

	/** Obtains a pointer to the server's ServerConfig object.
	 * The ServerConfig object contains most of the configuration data
	 * of the IRC server, as read from the config file by the core.
	 */
	ServerConfig* GetConfig();

	/** For use with Module::Prioritize().
	 * When the return value of this function is returned from
	 * Module::Prioritize(), this specifies that the module wishes
	 * to be ordered exactly BEFORE 'modulename'. For more information
	 * please see Module::Prioritize().
	 * @param modulename The module your module wants to be before in the call list
	 * @returns a priority ID which the core uses to relocate the module in the list
	 */
	long PriorityBefore(std::string modulename);

	/** For use with Module::Prioritize().
	 * When the return value of this function is returned from
	 * Module::Prioritize(), this specifies that the module wishes
	 * to be ordered exactly AFTER 'modulename'. For more information please
	 * see Module::Prioritize().
	 * @param modulename The module your module wants to be after in the call list
	 * @returns a priority ID which the core uses to relocate the module in the list
	 */
	long PriorityAfter(std::string modulename);
	
	/** Sends text to all opers.
	 * This method sends a server notice to all opers with the usermode +s.
	 */
	virtual void SendOpers(std::string s);

	/** Returns the version string of this server
	 */
	std::string GetVersion();

	/** Writes a log string.
	 * This method writes a line of text to the log. If the level given is lower than the
	 * level given in the configuration, this command has no effect.
	 */
	virtual void Log(int level, std::string s);

	/** Sends a line of text down a TCP/IP socket.
	 * This method writes a line of text to an established socket, cutting it to 510 characters
	 * plus a carriage return and linefeed if required.
	 */
	virtual void Send(int Socket, std::string s);

	/** Sends text from the server to a socket.
	 * This method writes a line of text to an established socket, with the servername prepended
	 * as used by numerics (see RFC 1459)
	 */
	virtual void SendServ(int Socket, std::string s);

	/** Writes text to a channel, but from a server, including all.
	 * This can be used to send server notices to a group of users.
	 */
	virtual void SendChannelServerNotice(std::string ServName, chanrec* Channel, std::string text);

	/** Sends text from a user to a socket.
	 * This method writes a line of text to an established socket, with the given user's nick/ident
	 * /host combination prepended, as used in PRIVSG etc commands (see RFC 1459)
	 */
	virtual void SendFrom(int Socket, userrec* User, std::string s);

	/** Sends text from a user to another user.
	 * This method writes a line of text to a user, with a user's nick/ident
	 * /host combination prepended, as used in PRIVMSG etc commands (see RFC 1459)
	 * If you specify NULL as the source, then the data will originate from the
	 * local server, e.g. instead of:
	 *
	 * :user!ident@host TEXT
	 *
	 * The format will become:
	 *
	 * :localserver TEXT
	 *
	 * Which is useful for numerics and server notices to single users, etc.
	 */
	virtual void SendTo(userrec* Source, userrec* Dest, std::string s);

	/** Sends text from a user to a channel (mulicast).
	 * This method writes a line of text to a channel, with the given user's nick/ident
	 * /host combination prepended, as used in PRIVMSG etc commands (see RFC 1459). If the
	 * IncludeSender flag is set, then the text is also sent back to the user from which
	 * it originated, as seen in MODE (see RFC 1459).
	 */
	virtual void SendChannel(userrec* User, chanrec* Channel, std::string s,bool IncludeSender);

	/** Returns true if two users share a common channel.
	 * This method is used internally by the NICK and QUIT commands, and the Server::SendCommon
	 * method.
	 */
	virtual bool CommonChannels(userrec* u1, userrec* u2);

	/** Sends text from a user to one or more channels (mulicast).
	 * This method writes a line of text to all users which share a common channel with a given	
	 * user, with the user's nick/ident/host combination prepended, as used in PRIVMSG etc
	 * commands (see RFC 1459). If the IncludeSender flag is set, then the text is also sent
	 * back to the user from which it originated, as seen in NICK (see RFC 1459). Otherwise, it
	 * is only sent to the other recipients, as seen in QUIT.
	 */
	virtual void SendCommon(userrec* User, std::string text,bool IncludeSender);

	/** Sends a WALLOPS message.
	 * This method writes a WALLOPS message to all users with the +w flag, originating from the
	 * specified user.
	 */
	virtual void SendWallops(userrec* User, std::string text);

	/** Returns true if a nick is valid.
	 * Nicks for unregistered connections will return false.
	 */
	virtual bool IsNick(std::string nick);

	/** Returns a count of the number of users on a channel.
	 * This will NEVER be 0, as if the chanrec exists, it will have at least one user in the channel.
	 */
	virtual int CountUsers(chanrec* c);

	/** Adds an InspTimer which will trigger at a future time
	 */
	virtual void AddTimer(InspTimer* T);

	/** Attempts to look up a nick and return a pointer to it.
	 * This function will return NULL if the nick does not exist.
	 */
	virtual userrec* FindNick(std::string nick);

	/** Attempts to look up a nick using the file descriptor associated with that nick.
	 * This function will return NULL if the file descriptor is not associated with a valid user.
	 */
	virtual userrec* FindDescriptor(int socket);

	/** Attempts to look up a channel and return a pointer to it.
	 * This function will return NULL if the channel does not exist.
	 */
	virtual chanrec* FindChannel(std::string channel);

	/** Attempts to look up a user's privilages on a channel.
	 * This function will return a string containing either @, %, +, or an empty string,
	 * representing the user's privilages upon the channel you specify.
	 */
	virtual std::string ChanMode(userrec* User, chanrec* Chan);

	/** Checks if a user is on a channel.
	 * This function will return true or false to indicate if user 'User' is on channel 'Chan'.
	 */
	virtual bool IsOnChannel(userrec* User, chanrec* Chan);

	/** Returns the server name of the server where the module is loaded.
	 */
	virtual std::string GetServerName();

	/** Returns the network name, global to all linked servers.
	 */
	virtual std::string GetNetworkName();

	/** Returns the server description string of the local server
	 */
	virtual std::string GetServerDescription();

	/** Returns the information of the server as returned by the /ADMIN command.
	 * See the Admin class for further information of the return value. The members
	 * Admin::Nick, Admin::Email and Admin::Name contain the information for the
	 * server where the module is loaded.
	 */
	virtual Admin GetAdmin();

	/** Adds an extended mode letter which is parsed by a module.
	 * This allows modules to add extra mode letters, e.g. +x for hostcloak.
	 * the "type" parameter is either MT_CHANNEL, MT_CLIENT, or MT_SERVER, to
	 * indicate wether the mode is a channel mode, a client mode, or a server mode.
	 * requires_oper is used with MT_CLIENT type modes only to indicate the mode can only
	 * be set or unset by an oper. If this is used for MT_CHANNEL type modes it is ignored.
	 * params_when_on is the number of modes to expect when the mode is turned on
	 * (for type MT_CHANNEL only), e.g. with mode +k, this would have a value of 1.
	 * the params_when_off value has a similar value to params_when_on, except it indicates
	 * the number of parameters to expect when the mode is disabled. Modes which act in a similar
	 * way to channel mode +l (e.g. require a parameter to enable, but not to disable) should
	 * use this parameter. The function returns false if the mode is unavailable, and will not
	 * attempt to allocate another character, as this will confuse users. This also means that
	 * as only one module can claim a specific mode character, the core does not need to keep track
	 * of which modules own which modes, which speeds up operation of the server. In this version,
	 * a mode can have at most one parameter, attempting to use more parameters will have undefined
	 * effects.
	 */
	virtual bool AddExtendedMode(char modechar, int type, bool requires_oper, int params_when_on, int params_when_off);

	/** Adds an extended mode letter which is parsed by a module and handled in a list fashion.
	 * This call is used to implement modes like +q and +a. The characteristics of these modes are
	 * as follows:
	 *
	 * (1) They are ALWAYS on channels, not on users, therefore their type is MT_CHANNEL
	 *
	 * (2) They always take exactly one parameter when being added or removed
	 *
	 * (3) They can be set multiple times, usually on users in channels
	 *
	 * (4) The mode and its parameter are NOT stored in the channels modes structure
	 *
	 * It is down to the module handling the mode to maintain state and determine what 'items' (e.g. users,
	 * or a banlist) have the mode set on them, and process the modes at the correct times, e.g. during access
	 * checks on channels, etc. When the extended mode is triggered the OnExtendedMode method will be triggered
	 * as above. Note that the target you are given will be a channel, if for example your mode is set 'on a user'
	 * (in for example +a) you must use Server::Find to locate the user the mode is operating on.
	 * Your mode handler may return 1 to handle the mode AND tell the core to display the mode change, e.g.
	 * '+aaa one two three' in the case of the mode for 'two', or it may return -1 to 'eat' the mode change,
	 * so the above example would become '+aa one three' after processing.
	 */
	virtual bool AddExtendedListMode(char modechar);

	/** Adds a command to the command table.
	 * This allows modules to add extra commands into the command table. You must place a function within your
	 * module which is is of type handlerfunc:
	 * 
	 * typedef void (handlerfunc) (char**, int, userrec*);
	 * ...
	 * void handle_kill(char **parameters, int pcnt, userrec *user)
	 *
	 * When the command is typed, the parameters will be placed into the parameters array (similar to argv) and
	 * the parameter count will be placed into pcnt (similar to argv). There will never be any less parameters
	 * than the 'minparams' value you specified when creating the command. The *user parameter is the class of
	 * the user which caused the command to trigger, who will always have the flag you specified in 'flags' when
	 * creating the initial command. For example to create an oper only command create the commands with flags='o'.
	 * The source parameter is used for resource tracking, and should contain the name of your module (with file
	 * extension) e.g. "m_blarp.so". If you place the wrong identifier here, you can cause crashes if your module
	 * is unloaded.
	 */
	virtual void AddCommand(command_t *f);
	 
 	/** Sends a servermode.
 	 * you must format the parameters array with the target, modes and parameters for those modes.
 	 *
 	 * For example:
 	 *
 	 * char *modes[3];
 	 *
	 * modes[0] = ChannelName;
	 *
	 * modes[1] = "+o";
	 *
	 * modes[2] = user->nick;
	 *
	 * Srv->SendMode(modes,3,user);
	 *
	 * The modes will originate from the server where the command was issued, however responses (e.g. numerics)
	 * will be sent to the user you provide as the third parameter.
	 * You must be sure to get the number of parameters correct in the pcnt parameter otherwise you could leave
	 * your server in an unstable state!
	 */

  	virtual void SendMode(char **parameters, int pcnt, userrec *user);
  	
  	/** Sends to all users matching a mode mask
  	 * You must specify one or more usermodes as the first parameter. These can be RFC specified modes such as +i,
  	 * or module provided modes, including ones provided by your own module.
  	 * In the second parameter you must place a flag value which indicates wether the modes you have given will be
  	 * logically ANDed or OR'ed. You may use one of either WM_AND or WM_OR.
  	 * for example, if you were to use:
  	 *
  	 * Serv->SendToModeMask("xi", WM_OR, "m00");
  	 *
  	 * Then the text 'm00' will be sent to all users with EITHER mode x or i. Conversely if you used WM_AND, the
  	 * user must have both modes set to receive the message.
  	 */
  	virtual void SendToModeMask(std::string modes, int flags, std::string text);

	/** Forces a user to join a channel.
	 * This is similar to svsjoin and can be used to implement redirection, etc.
	 * On success, the return value is a valid pointer to a chanrec* of the channel the user was joined to.
	 * On failure, the result is NULL.
	 */
	virtual chanrec* JoinUserToChannel(userrec* user, std::string cname, std::string key);
	
	/** Forces a user to part a channel.
	 * This is similar to svspart and can be used to implement redirection, etc.
	 * Although the return value of this function is a pointer to a channel record, the returned data is
	 * undefined and should not be read or written to. This behaviour may be changed in a future version.
	 */
	virtual chanrec* PartUserFromChannel(userrec* user, std::string cname, std::string reason);
	
	/** Forces a user nickchange.
	 * This command works similarly to SVSNICK, and can be used to implement Q-lines etc.
	 * If you specify an invalid nickname, the nick change will be dropped and the target user will receive
	 * the error numeric for it.
	 */
	virtual void ChangeUserNick(userrec* user, std::string nickname);
	
	/** Forces a user to quit with the specified reason.
	 * To the user, it will appear as if they typed /QUIT themselves, except for the fact that this function
	 * may bypass the quit prefix specified in the config file.
	 *
	 * WARNING!
	 *
	 * Once you call this function, userrec* user will immediately become INVALID. You MUST NOT write to, or
	 * read from this pointer after calling the QuitUser method UNDER ANY CIRCUMSTANCES! The best course of
	 * action after calling this method is to immediately bail from your handler.
	 */
	virtual void QuitUser(userrec* user, std::string reason);

	/** Makes a user kick another user, with the specified reason.
	 * If source is NULL, the server will peform the kick.
	 * @param The person or server (if NULL) performing the KICK
	 * @param target The person being kicked
	 * @param chan The channel to kick from
	 * @param reason The kick reason
	 */
	virtual void KickUser(userrec* source, userrec* target, chanrec* chan, std::string reason);
	
	/**  Matches text against a glob pattern.
	 * Uses the ircd's internal matching function to match string against a globbing pattern, e.g. *!*@*.com
	 * Returns true if the literal successfully matches the pattern, false if otherwise.
	 */
	virtual bool MatchText(std::string sliteral, std::string spattern);
	
	/** Calls the handler for a command, either implemented by the core or by another module.
	 * You can use this function to trigger other commands in the ircd, such as PRIVMSG, JOIN,
	 * KICK etc, or even as a method of callback. By defining command names that are untypeable
	 * for users on irc (e.g. those which contain a \r or \n) you may use them as callback identifiers.
	 * The first parameter to this method is the name of the command handler you wish to call, e.g.
	 * PRIVMSG. This will be a command handler previously registered by the core or wih AddCommand().
	 * The second parameter is an array of parameters, and the third parameter is a count of parameters
	 * in the array. If you do not pass enough parameters to meet the minimum needed by the handler, the
	 * functiom will silently ignore it. The final parameter is the user executing the command handler,
	 * used for privilage checks, etc.
	 * @return True if the command exists
	 */
	virtual bool CallCommandHandler(std::string commandname, char** parameters, int pcnt, userrec* user);

	/** This function returns true if the commandname exists, pcnt is equal to or greater than the number
	 * of paramters the command requires, the user specified is allowed to execute the command, AND
	 * if the command is implemented by a module (not the core). This has a few specific uses, usually
	 * within network protocols (see src/modules/m_spanningtree.cpp)
	 */
	virtual bool IsValidModuleCommand(std::string commandname, int pcnt, userrec* user);
	
	/** Change displayed hostname of a user.
	 * You should always call this method to change a user's host rather than writing directly to the
	 * dhost member of userrec, as any change applied via this method will be propogated to any
	 * linked servers.
	 */	
	virtual void ChangeHost(userrec* user, std::string host);
	
	/** Change GECOS (fullname) of a user.
	 * You should always call this method to change a user's GECOS rather than writing directly to the
	 * fullname member of userrec, as any change applied via this method will be propogated to any
	 * linked servers.
	 */	
	virtual void ChangeGECOS(userrec* user, std::string gecos);
	
	/** Returns true if the servername you give is ulined.
	 * ULined servers have extra privilages. They are allowed to change nicknames on remote servers,
	 * change modes of clients which are on remote servers and set modes of channels where there are
	 * no channel operators for that channel on the ulined server, amongst other things.
	 */
	virtual bool IsUlined(std::string server);
	
	/** Fetches the userlist of a channel. This function must be here and not a member of userrec or
	 * chanrec due to include constraints.
	 */
	virtual chanuserlist GetUsers(chanrec* chan);

	/** Remove a user's connection to the irc server, but leave their client in existence in the
	 * user hash. When you call this function, the user's file descriptor will be replaced with the
	 * value of FD_MAGIC_NUMBER and their old file descriptor will be closed. This idle client will
	 * remain until it is restored with a valid file descriptor, or is removed from IRC by an operator
	 * After this call, the pointer to user will be invalid.
	 */
	virtual bool UserToPseudo(userrec* user,std::string message);

	/** This user takes one user, and switches their file descriptor with another user, so that one user
	 * "becomes" the other. The user in 'alive' is booted off the server with the given message. The user
	 * referred to by 'zombie' should have previously been locked with Server::UserToPseudo, otherwise
	 * stale sockets and file descriptor leaks can occur. After this call, the pointer to alive will be
	 * invalid, and the pointer to zombie will be equivalent in effect to the old pointer to alive.
	 */
	virtual bool PseudoToUser(userrec* alive,userrec* zombie,std::string message);

	/** Adds a G-line
	 * The G-line is propogated to all of the servers in the mesh and enforced as soon as it is added.
	 * The duration must be in seconds, however you can use the Server::CalcDuration method to convert
	 * durations into the 1w2d3h3m6s format used by /GLINE etc. The source is an arbitary string used
	 * to indicate who or what sent the data, usually this is the nickname of a person, or a server
	 * name.
	 */
	virtual void AddGLine(long duration, std::string source, std::string reason, std::string hostmask);

        /** Adds a Q-line
         * The Q-line is propogated to all of the servers in the mesh and enforced as soon as it is added.
         * The duration must be in seconds, however you can use the Server::CalcDuration method to convert
         * durations into the 1w2d3h3m6s format used by /GLINE etc. The source is an arbitary string used
         * to indicate who or what sent the data, usually this is the nickname of a person, or a server
         * name.
         */
	virtual void AddQLine(long duration, std::string source, std::string reason, std::string nickname);

        /** Adds a Z-line
         * The Z-line is propogated to all of the servers in the mesh and enforced as soon as it is added.
         * The duration must be in seconds, however you can use the Server::CalcDuration method to convert
         * durations into the 1w2d3h3m6s format used by /GLINE etc. The source is an arbitary string used
         * to indicate who or what sent the data, usually this is the nickname of a person, or a server
         * name.
         */
	virtual void AddZLine(long duration, std::string source, std::string reason, std::string ipaddr);

        /** Adds a K-line
         * The K-line is enforced as soon as it is added.
         * The duration must be in seconds, however you can use the Server::CalcDuration method to convert
         * durations into the 1w2d3h3m6s format used by /GLINE etc. The source is an arbitary string used
         * to indicate who or what sent the data, usually this is the nickname of a person, or a server
         * name.
         */
	virtual void AddKLine(long duration, std::string source, std::string reason, std::string hostmask);

        /** Adds a E-line
         * The E-line is enforced as soon as it is added.
         * The duration must be in seconds, however you can use the Server::CalcDuration method to convert
         * durations into the 1w2d3h3m6s format used by /GLINE etc. The source is an arbitary string used
         * to indicate who or what sent the data, usually this is the nickname of a person, or a server
         * name.
         */
	virtual void AddELine(long duration, std::string source, std::string reason, std::string hostmask);

	/** Deletes a G-Line from all servers
	 */
	virtual bool DelGLine(std::string hostmask);

	/** Deletes a Q-Line from all servers
	 */
	virtual bool DelQLine(std::string nickname);

	/** Deletes a Z-Line from all servers
	 */
	virtual bool DelZLine(std::string ipaddr);

	/** Deletes a local K-Line
	 */
	virtual bool DelKLine(std::string hostmask);

	/** Deletes a local E-Line
	 */
	virtual bool DelELine(std::string hostmask);

	/** Calculates a duration
	 * This method will take a string containing a formatted duration (e.g. "1w2d") and return its value
	 * as a total number of seconds. This is the same function used internally by /GLINE etc to set
	 * the ban times.
	 */
	virtual long CalcDuration(std::string duration);

	/** Returns true if a nick!ident@host string is correctly formatted, false if otherwise.
	 */
	virtual bool IsValidMask(std::string mask);

	/** This function finds a module by name.
	 * You must provide the filename of the module. If the module cannot be found (is not loaded)
	 * the function will return NULL.
	 */
	virtual Module* FindModule(std::string name);

	/** Adds a class derived from InspSocket to the server's socket engine.
	 */
	virtual void AddSocket(InspSocket* sock);

	/** Forcibly removes a class derived from InspSocket from the servers socket engine.
	 */
	virtual void RemoveSocket(InspSocket* sock);

	/** Deletes a class derived from InspSocket from the server's socket engine.
	 */
	virtual void DelSocket(InspSocket* sock);

	virtual void RehashServer();

	virtual long GetChannelCount();

	virtual chanrec* GetChannelIndex(long index);
};


#define CONF_NOT_A_NUMBER	0x000010
#define CONF_NOT_UNSIGNED	0x000080
#define CONF_VALUE_NOT_FOUND	0x000100
#define CONF_FILE_NOT_FOUND	0x000200


/** Allows reading of values from configuration files
 * This class allows a module to read from either the main configuration file (inspircd.conf) or from
 * a module-specified configuration file. It may either be instantiated with one parameter or none.
 * Constructing the class using one parameter allows you to specify a path to your own configuration
 * file, otherwise, inspircd.conf is read.
 */
class ConfigReader : public classbase
{
  protected:
	/** The contents of the configuration file
	 * This protected member should never be accessed by a module (and cannot be accessed unless the
	 * core is changed). It will contain a pointer to the configuration file data with unneeded data
	 * (such as comments) stripped from it.
	 */
	std::stringstream *cache;
	std::stringstream *errorlog;
	/** Used to store errors
	 */
	bool readerror;
	long error;
	
  public:
	/** Default constructor.
	 * This constructor initialises the ConfigReader class to read the inspircd.conf file
	 * as specified when running ./configure.
	 */
	ConfigReader(); 		// default constructor reads ircd.conf
	/** Overloaded constructor.
	 * This constructor initialises the ConfigReader class to read a user-specified config file
	 */
	ConfigReader(std::string filename);	// read a module-specific config
	/** Default destructor.
	 * This method destroys the ConfigReader class.
	 */
	~ConfigReader();
	/** Retrieves a value from the config file.
	 * This method retrieves a value from the config file. Where multiple copies of the tag
	 * exist in the config file, index indicates which of the values to retrieve.
	 */
	std::string ReadValue(std::string tag, std::string name, int index);
	/** Retrieves a boolean value from the config file.
	 * This method retrieves a boolean value from the config file. Where multiple copies of the tag
	 * exist in the config file, index indicates which of the values to retrieve. The values "1", "yes"
	 * and "true" in the config file count as true to ReadFlag, and any other value counts as false.
	 */
	bool ReadFlag(std::string tag, std::string name, int index);
	/** Retrieves an integer value from the config file.
	 * This method retrieves an integer value from the config file. Where multiple copies of the tag
	 * exist in the config file, index indicates which of the values to retrieve. Any invalid integer
	 * values in the tag will cause the objects error value to be set, and any call to GetError() will
	 * return CONF_INVALID_NUMBER to be returned. needs_unsigned is set if the number must be unsigned.
	 * If a signed number is placed into a tag which is specified unsigned, 0 will be returned and GetError()
	 * will return CONF_NOT_UNSIGNED
	 */
	long ReadInteger(std::string tag, std::string name, int index, bool needs_unsigned);
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
	int Enumerate(std::string tag);
	/** Returns true if a config file is valid.
	 * This method is partially implemented and will only return false if the config
	 * file does not exist or could not be opened.
	 */
	bool Verify();
	/** Dumps the list of errors in a config file to an output location. If bail is true,
	 * then the program will abort. If bail is false and user points to a valid user
	 * record, the error report will be spooled to the given user by means of NOTICE.
	 * if bool is false AND user is false, the error report will be spooled to all opers
	 * by means of a NOTICE to all opers.
	 */
	void DumpErrors(bool bail,userrec* user);

	/** Returns the number of items within a tag.
	 * For example if the tag was &lt;test tag="blah" data="foo"&gt; then this
	 * function would return 2. Spaces and newlines both qualify as valid seperators
	 * between values.
	 */
	int EnumerateValues(std::string tag, int index);
};



/** Caches a text file into memory and can be used to retrieve lines from it.
 * This class contains methods for read-only manipulation of a text file in memory.
 * Either use the constructor type with one parameter to load a file into memory
 * at construction, or use the LoadFile method to load a file.
 */
class FileReader : public classbase
{
 file_cache fc;
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
	 FileReader(std::string filename);

	 /** Default destructor.
	  * This deletes the memory allocated to the file.
	  */
	 ~FileReader();

	 /** Used to load a file.
	  * This method loads a file into the class ready for GetLine and
	  * and other methods to be called. If the file could not be loaded, FileReader::FileSize
	  * returns 0.
	  */
	 void LoadFile(std::string filename);

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


/** Instantiates classes inherited from Module
 * This class creates a class inherited from type Module, using new. This is to allow for modules
 * to create many different variants of Module, dependent on architecture, configuration, etc.
 * In most cases, the simple class shown in the example module m_foobar.so will suffice for most
 * modules.
 */
class ModuleFactory : public classbase
{
 public:
	ModuleFactory() { }
	virtual ~ModuleFactory() { }
	/** Creates a new module.
	 * Your inherited class of ModuleFactory must return a pointer to your Module class
	 * using this method.
	 */
	virtual Module * CreateModule(Server* Me) = 0;
};


typedef DLLFactory<ModuleFactory> ircd_module;

bool ModeDefined(char c, int i);
bool ModeDefinedOper(char c, int i);
int ModeDefinedOn(char c, int i);
int ModeDefinedOff(char c, int i);
void ModeMakeList(char modechar);
bool ModeIsListMode(char modechar, int type);

#endif
