/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
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


#ifndef __PLUGIN_H
#define __PLUGIN_H

// log levels

#define DEBUG 10
#define VERBOSE 20
#define DEFAULT 30
#define SPARSE 40
#define NONE 50

// used with OnExtendedMode() method of modules

#define MT_CHANNEL 1
#define MT_CLIENT 2
#define MT_SERVER 3

// used with OnAccessCheck() method of modules

#define ACR_DEFAULT 0		// Do default action (act as if the module isnt even loaded)
#define ACR_DENY 1		// deny the action
#define ACR_ALLOW 2		// allow the action

#define AC_KICK 0		// a user is being kicked
#define AC_DEOP 1		// a user is being deopped
#define AC_OP 2			// a user is being opped
#define AC_VOICE 3		// a user is being voiced
#define AC_DEVOICE 4		// a user is being devoiced
#define AC_HALFOP 5		// a user is being halfopped
#define AC_DEHALFOP 6		// a user is being dehalfopped
#define AC_INVITE 7		// a user is being invited
#define AC_GENERAL_MODE 8	// a user channel mode is being changed

// used to define a set of behavior bits for a module

#define VF_STATIC		1	// module is static, cannot be /unloadmodule'd
#define VF_VENDOR		2	// module is a vendor module (came in the original tarball, not 3rd party)
#define VF_SERVICEPROVIDER	4	// module provides a service to other modules (can be a dependency)
#define VF_COMMON		8	// module needs to be common on all servers in a mesh to link

#include "dynamic.h"
#include "base.h"
#include "ctables.h"
#include <string>
#include <deque>
#include <sstream>

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

#define FOREACH_MOD for (int i = 0; i <= MODCOUNT; i++) modules[i]->

// This define is similar to the one above but returns a result in MOD_RESULT.
// The first module to return a nonzero result is the value to be accepted,
// and any modules after are ignored.

// *********************************************************************************************

#define FOREACH_RESULT(x) { MOD_RESULT = 0; \
			for (int i = 0; i <= MODCOUNT; i++) { \
			int res = modules[i]->x ; \
			if (res != 0) { \
				MOD_RESULT = res; \
				break; \
			} \
		} \
	} 
   
// *********************************************************************************************

#define FD_MAGIC_NUMBER -42

extern void createcommand(char* cmd, handlerfunc f, char flags, int minparams, char* source);
extern void server_mode(char **parameters, int pcnt, userrec *user);

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

/** Base class for all InspIRCd modules
 *  This class is the base class for InspIRCd modules. All modules must inherit from this class,
 *  its methods will be called when irc server events occur. class inherited from module must be
 *  instantiated by the ModuleFactory class (see relevent section) for the plugin to be initialised.
 */
class Module : public classbase
{
 public:

	/** Default constructor
	 * creates a module class
	 */
	Module();

	/** Default destructor
	 * destroys a module class
	 */
	virtual ~Module();

	/** Returns the version number of a Module.
	 * The method should return a Version object with its version information assigned via
	 * Version::Version
	 */
	virtual Version GetVersion();

	/** Called when a user connects.
	 * The details of the connecting user are available to you in the parameter userrec *user
	 */
	virtual void OnUserConnect(userrec* user);

	/** Called when a user quits.
	 * The details of the exiting user are available to you in the parameter userrec *user
	 */
	virtual void OnUserQuit(userrec* user);

	/** Called when a user joins a channel.
	 * The details of the joining user are available to you in the parameter userrec *user,
	 * and the details of the channel they have joined is available in the variable chanrec *channel
	 */
	virtual void OnUserJoin(userrec* user, chanrec* channel);

	/** Called when a user parts a channel.
	 * The details of the leaving user are available to you in the parameter userrec *user,
	 * and the details of the channel they have left is available in the variable chanrec *channel
	 */
	virtual void OnUserPart(userrec* user, chanrec* channel);

	/** Called before a packet is transmitted across the irc network between two irc servers.
	 * This allows you to easily represent it in the correct ways to implement encryption, compression,
	 * digital signatures and anything else you may want to add. This should be regarded as a pre-processor
	 * and will be called before ANY other operations within the ircd core program.
	 */
	virtual void OnPacketTransmit(std::string &data, std::string serv);

	/** Called after a packet is received from another irc server.
	 * This allows you to easily represent it in the correct ways to implement encryption, compression,
	 * digital signatures and anything else you may want to add. This should be regarded as a pre-processor
	 * and will be called immediately after the packet is received but before any other operations with the
	 * core of the ircd.
	 */
 	virtual void OnPacketReceive(std::string &data, std::string serv);

	/** Called on rehash.
	 * This method is called prior to a /REHASH or when a SIGHUP is received from the operating
	 * system. You should use it to reload any files so that your module keeps in step with the
	 * rest of the application.
	 */
 	virtual void OnRehash();

	/** Called when a raw command is transmitted or received.
	 * This method is the lowest level of handler available to a module. It will be called with raw
	 * data which is passing through a connected socket. If you wish, you may munge this data by changing
	 * the string parameter "raw". If you do this, after your function exits it will immediately be
	 * cut down to 510 characters plus a carriage return and linefeed. For INBOUND messages only (where
	 * inbound is set to true) the value of user will be the userrec of the connection sending the
	 * data. This is not possible for outbound data because the data may be being routed to multiple targets.
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
	 */
	virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname);
	
        /** Called whenever a user is about to be kicked.
         * Returning a value of 1 from this function stops the process immediately, causing no
         * output to be sent to the user by the core. If you do this you must produce your own numerics,
         * notices etc.
         */
	virtual int OnUserPreKick(userrec* source, userrec* user, chanrec* chan, std::string reason);

	/** Called whenever a user is kicked.
	 * If this method is called, the kick is already underway and cannot be prevented, so
	 * to prevent a kick, please use Module::OnUserPreKick instead of this method.
	 */
	virtual void OnUserKick(userrec* source, userrec* user, chanrec* chan, std::string reason);

	/** Called whenever a user opers locally.
	 * The userrec will contain the oper mode 'o' as this function is called after any modifications
	 * are made to the user's structure by the core.
	 */
	virtual void OnOper(userrec* user);
	
	/** Called whenever a user types /INFO.
	 * The userrec will contain the information of the user who typed the command. Modules may use this
	 * method to output their own credits in /INFO (which is the ircd's version of an about box).
	 * It is purposefully not possible to modify any info that has already been output, or halt the list.
	 * You must write a 371 numeric to the user, containing your info in the following format:
	 *
	 * &lt;nick&gt; :information here
	 */
	virtual void OnInfo(userrec* user);
	
	/** Called whenever a /WHOIS is performed on a local user.
	 * The source parameter contains the details of the user who issued the WHOIS command, and
	 * the dest parameter contains the information of the user they are whoising.
	 */
	virtual void OnWhois(userrec* source, userrec* dest);
	
	/** Called whenever a user is about to invite another user into a channel, before any processing is done.
	 * Returning 1 from this function stops the process immediately, causing no
	 * output to be sent to the user by the core. If you do this you must produce your own numerics,
	 * notices etc. This is useful for modules which may want to filter invites to channels.
	 */
	virtual int OnUserPreInvite(userrec* source,userrec* dest,chanrec* channel);
	
	/** Called whenever a user is about to PRIVMSG A user or a channel, before any processing is done.
	 * Returning any nonzero value from this function stops the process immediately, causing no
	 * output to be sent to the user by the core. If you do this you must produce your own numerics,
	 * notices etc. This is useful for modules which may want to filter or redirect messages.
	 * target_type can be one of TYPE_USER or TYPE_CHANNEL. If the target_type value is a user,
	 * you must cast dest to a userrec* otherwise you must cast it to a chanrec*, this is the details
	 * of where the message is destined to be sent.
	 */
	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text);

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
	 */
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text);
	
	/** Called before any nickchange, local or remote. This can be used to implement Q-lines etc.
	 * Please note that although you can see remote nickchanges through this function, you should
	 * NOT make any changes to the userrec if the user is a remote user as this may cause a desnyc.
	 * check user->server before taking any action (including returning nonzero from the method).
	 * If your method returns nonzero, the nickchange is silently forbidden, and it is down to your
	 * module to generate some meaninful output.
	 */
	virtual int OnUserPreNick(userrec* user, std::string newnick);
	
	/** Called after any nickchange, local or remote. This can be used to track users after nickchanges
	 * have been applied. Please note that although you can see remote nickchanges through this function, you should
         * NOT make any changes to the userrec if the user is a remote user as this may cause a desnyc.
         * check user->server before taking any action (including returning nonzero from the method).
	 * Because this method is called after the nickchange is taken place, no return values are possible
	 * to indicate forbidding of the nick change. Use OnUserPreNick for this.
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
	 */
	virtual int OnAccessCheck(userrec* source,userrec* dest,chanrec* channel,int access_type);

	/** Called during a netburst to sync user data.
	 * This is called during the netburst on a per-user basis. You should use this call to up any special
	 * user-related things which are implemented by your module, e.g. sending listmodes. You may return
	 * multiple commands in the string_list.
	 */
	virtual string_list OnUserSync(userrec* user);

	/** Called during a netburst to sync channel data.
	 * This is called during the netburst on a per-channel basis. You should use this call to up any special
	 * channel-related things which are implemented by your module, e.g. sending listmodes. You may return
	 * multiple commands in the string_list.
	 */
	virtual string_list OnChannelSync(chanrec* chan);

	/** Called when a 005 numeric is about to be output.
	 * The module should modify the 005 numeric if needed to indicate its features.
	 */
	virtual void On005Numeric(std::string &output);

	/** Called when a client is disconnected by KILL.
	 * If a client is killed by a server, e.g. a nickname collision or protocol error,
	 * source is NULL.
	 * Return 1 from this function to prevent the kill, and 0 from this function to allow
	 * it as normal. If you prevent the kill no output will be sent to the client, it is
	 * down to your module to generate this information.
	 * NOTE: It is NOT advisable to stop kills which originate from servers. If you do
	 * so youre risking race conditions, desyncs and worse!
	 */
	virtual int OnKill(userrec* source, userrec* dest, std::string reason);

	/** Called whenever a module is loaded.
	 * mod will contain a pointer to the module, and string will contain its name,
	 * for example m_widgets.so. This function is primary for dependency checking,
	 * your module may decide to enable some extra features if it sees that you have
	 * for example loaded "m_killwidgets.so" with "m_makewidgets.so". It is highly
	 * recommended that modules do *NOT* bail if they cannot satisfy dependencies,
	 * but instead operate under reduced functionality, unless the dependency is
	 * absolutely neccessary (e.g. a module that extends the features of another
	 * module).
	 */
	virtual void OnLoadModule(Module* mod,std::string name);

	/** Called once every five seconds for background processing.
	 * This timer can be used to control timed features. Its period is not accurate
	 * enough to be used as a clock, but it is gauranteed to be called at least once in
	 * any five second period, directly from the main loop of the server.
	 */
	virtual void OnBackgroundTimer(time_t curtime);

	/** Called whenever a list is needed for a listmode.
	 * For example, when a /MODE #channel +b (without any other parameters) is called,
	 * if a module was handling +b this function would be called. The function can then
	 * output any lists it wishes to. Please note that all modules will see all mode
	 * characters to provide the ability to extend each other, so please only output
	 * a list if the mode character given matches the one(s) you want to handle.
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
	 */
	virtual int OnPreCommand(std::string command, char **parameters, int pcnt, userrec *user);

	/** Called to check if a user who is connecting can now be allowed to register
	 * If any modules return false for this function, the user is held in the waiting
	 * state until all modules return true. For example a module which implements ident
	 * lookups will continue to return false for a user until their ident lookup is completed.
	 * Note that the registration timeout for a user overrides these checks, if the registration
	 * timeout is reached, the user is disconnected even if modules report that the user is
	 * not ready to connect.
	 */
	virtual bool OnCheckReady(userrec* user);

	/** Called whenever a user is about to register their connection (e.g. before the user
	 * is sent the MOTD etc). Modules can use this method if they are performing a function
	 * which must be done before the actual connection is completed (e.g. ident lookups,
	 * dnsbl lookups, etc).
	 * Note that you should NOT delete the user record here by causing a disconnection!
	 * Use OnUserConnect for that instead.
	 */
	virtual void OnUserRegister(userrec* user);

	/** Called whenever a mode character is processed.
	 * Return 1 from this function to block the mode character from being processed entirely,
	 * so that you may perform your own code instead. Note that this method allows you to override
	 * modes defined by other modes, but this is NOT RECOMMENDED!
	 */
	virtual int OnRawMode(userrec* user, chanrec* chan, char mode, std::string param, bool adding, int pcnt);

	/** Called whenever a user joins a channel, to determine if invite checks should go ahead or not.
	 * This method will always be called for each join, wether or not the channel is actually +i, and
	 * determines the outcome of an if statement around the whole section of invite checking code.
	 * return 1 to explicitly allow the join to go ahead or 0 to ignore the event.
	 */
	virtual int OnCheckInvite(userrec* user, chanrec* chan);

        /** Called whenever a user joins a channel, to determine if key checks should go ahead or not.
         * This method will always be called for each join, wether or not the channel is actually +k, and
         * determines the outcome of an if statement around the whole section of key checking code.
	 * if the user specified no key, the keygiven string will be a valid but empty value.
         * return 1 to explicitly allow the join to go ahead or 0 to ignore the event.
         */
	virtual int OnCheckKey(userrec* user, chanrec* chan, std::string keygiven);

        /** Called whenever a user joins a channel, to determine if channel limit checks should go ahead or not.
         * This method will always be called for each join, wether or not the channel is actually +l, and
         * determines the outcome of an if statement around the whole section of channel limit checking code.
         * return 1 to explicitly allow the join to go ahead or 0 to ignore the event.
         */
	virtual int OnCheckLimit(userrec* user, chanrec* chan);

        /** Called whenever a user joins a channel, to determine if banlist checks should go ahead or not.
         * This method will always be called for each join, wether or not the user actually matches a channel ban, and
         * determines the outcome of an if statement around the whole section of ban checking code.
         * return 1 to explicitly allow the join to go ahead or 0 to ignore the event.
         */
	virtual int OnCheckBan(userrec* user, chanrec* chan);

	/** Called on all /STATS commands
	 * This method is triggered for all /STATS use, including stats symbols handled by the core.
	 */
	virtual void OnStats(char symbol);

	/** Called whenever a change of a local users displayed host is attempted.
	 * Return 1 to deny the host change, or 0 to allow it.
	 */
	virtual int OnChangeLocalUserHost(userrec* user, std::string newhost);

	/** Called whenever a change of a local users GECOS (fullname field) is attempted.
	 * return 1 to deny the name change, or 0 to allow it.
	 */
	virtual int OnChangeLocalUserGECOS(userrec* user, std::string newhost); 

	/** Called whenever a topic is changed by a local user.
	 * Return 1 to deny the topic change, or 0 to allow it.
	 */
	virtual int OnLocalTopicChange(userrec* user, chanrec* chan, std::string topic);
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

	/** Sends text to all opers.
	 * This method sends a server notice to all opers with the usermode +s.
	 */
	virtual void SendOpers(std::string s);
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
	/** Attempts to look up a nick and return a pointer to it.
	 * This function will return NULL if the nick does not exist.
	 */
	virtual userrec* FindNick(std::string nick);
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
	virtual void AddCommand(char* cmd, handlerfunc f, char flags, int minparams, char* source);
	 
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
	 */
	virtual void CallCommandHandler(std::string commandname, char** parameters, int pcnt, userrec* user);
	
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
	 * no channel operators for that channel on the ulined server, amongst other things. Ulined server
	 * data is also broadcast across the mesh at all times as opposed to selectively messaged in the
	 * case of normal servers, as many ulined server types (such as services) do not support meshed
	 * links and must operate in this manner.
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
	 * referred to by 'zombie' should have previously been locked with Server::ZombifyUser, otherwise
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

	/** Deletes a G-Line from all servers on the mesh
	 */
	virtual bool DelGLine(std::string hostmask);

	/** Deletes a Q-Line from all servers on the mesh
	 */
	virtual bool DelQLine(std::string nickname);

	/** Deletes a Z-Line from all servers on the mesh
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
	virtual Module * CreateModule() = 0;
};


typedef DLLFactory<ModuleFactory> ircd_module;

#endif
