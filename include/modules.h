/*



*/


#ifndef __PLUGIN_H
#define __PLUGIN_H

#define DEBUG 10
#define VERBOSE 20
#define DEFAULT 30
#define SPARSE 40
#define NONE 50

#define MT_CHANNEL 1
#define MT_CLIENT 2
#define MT_SERVER 3

#include "dynamic.h"
#include "base.h"
#include "ctables.h"
#include <string>
#include <deque>

/** Low level definition of a FileReader classes file cache area
 */
typedef std::deque<std::string> file_cache;
typedef file_cache string_list;

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
			if (res) { \
				MOD_RESULT = res; \
				break; \
			} \
		} \
   } 
   
// *********************************************************************************************

extern void createcommand(char* cmd, handlerfunc f, char flags, int minparams);
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
	 const int Major, Minor, Revision, Build;
	 Version(int major, int minor, int revision, int build);
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
	 * The packet is represented as a char*, as it should be regarded as a buffer, and not a string.
	 * This allows you to easily represent it in the correct ways to implement encryption, compression,
	 * digital signatures and anything else you may want to add. This should be regarded as a pre-processor
	 * and will be called before ANY other operations within the ircd core program.
	 */
	virtual void OnPacketTransmit(char *p);

	/** Called after a packet is received from another irc server.
	 * The packet is represented as a char*, as it should be regarded as a buffer, and not a string.
	 * This allows you to easily represent it in the correct ways to implement encryption, compression,
	 * digital signatures and anything else you may want to add. This should be regarded as a pre-processor
	 * and will be called immediately after the packet is received but before any other operations with the
	 * core of the ircd.
	 */
 	virtual void OnPacketReceive(char *p);

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
	 * cut down to 510 characters plus a carriage return and linefeed.
	 */
 	virtual void OnServerRaw(std::string &raw, bool inbound);

	/** Called whenever an extended mode is to be processed.
	 * The type parameter is MT_SERVER, MT_CLIENT or MT_CHANNEL, dependent on where the mode is being
	 * changed. mode_on is set when the mode is being set, in which case params contains a list of
	 * parameters for the mode as strings. If mode_on is false, the mode is being removed, and parameters
	 * may contain the parameters for the mode, dependent on wether they were defined when a mode handler
 	 * was set up with Server::AddExtendedMode
 	 * If the mode is a channel mode, target is a chanrec*, and if it is a user mode, target is a userrec*.
 	 * You must cast this value yourself to make use of it.
	 */
 	virtual bool OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params);
 	
	/** Called whenever a user is about to join a channel, before any processing is done.
	 * Returning any nonzero value from this function stops the process immediately, causing no
	 * output to be sent to the user by the core. If you do this you must produce your own numerics,
	 * notices etc. This is useful for modules which may want to mimic +b, +k, +l etc.
	 *
	 * IMPORTANT NOTE!
	 *
	 * If the user joins a NEW channel which does not exist yet, OnUserPreJoin will be called BEFORE the channel
	 * record is created. This will cause chanrec* chan to be NULL. There is very little you can do in form of
	 * processing on the actual channel record at this point, however the channel NAME will still be passed in
	 * char* cname, so that you could for example implement a channel blacklist or whitelist, etc.
	 */
	virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname);
	
	
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
	 * <nick> :information here
	 */
	virtual void OnInfo(userrec* user);
	
	/** Called whenever a /WHOIS is performed on a local user.
	 * The source parameter contains the details of the user who issued the WHOIS command, and
	 * the dest parameter contains the information of the user they are whoising.
	 */
	virtual void Module::OnWhois(userrec* source, userrec* dest);
	
	/** Called whenever a user is about to PRIVMSG A user or a channel, before any processing is done.
	 * Returning any nonzero value from this function stops the process immediately, causing no
	 * output to be sent to the user by the core. If you do this you must produce your own numerics,
	 * notices etc. This is useful for modules which may want to filter or redirect messages.
	 * target_type can be one of TYPE_USER or TYPE_CHANNEL. If the target_type value is a user,
	 * you must cast dest to a userrec* otherwise you must cast it to a chanrec*, this is the details
	 * of where the message is destined to be sent.
	 */
	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::String text);

	/** Called whenever a user is about to NOTICE A user or a channel, before any processing is done.
	 * Returning any nonzero value from this function stops the process immediately, causing no
	 * output to be sent to the user by the core. If you do this you must produce your own numerics,
	 * notices etc. This is useful for modules which may want to filter or redirect messages.
	 * target_type can be one of TYPE_USER or TYPE_CHANNEL. If the target_type value is a user,
	 * you must cast dest to a userrec* otherwise you must cast it to a chanrec*, this is the details
	 * of where the message is destined to be sent.
	 */
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::String text);
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
	/** Adds an extended mode letter which is parsed by a module
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
	 */
	virtual void AddCommand(char* cmd, handlerfunc f, char flags, int minparams);
	 
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
	virtual chanrec* Server::JoinUserToChannel(userrec* user, std::string cname, std::string key);
	
	/** Forces a user to part a channel.
	 * This is similar to svspart and can be used to implement redirection, etc.
	 * Although the return value of this function is a pointer to a channel record, the returned data is
	 * undefined and should not be read or written to. This behaviour may be changed in a future version.
	 */
	virtual chanrec* Server::PartUserFromChannel(userrec* user, std::string cname, std::string reason);
	
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
};

/** Allows reading of values from configuration files
 * This class allows a module to read from either the main configuration file (inspircd.conf) or from
 * a module-specified configuration file. It may either be instantiated with one parameter or none.
 * Constructing the class using one parameter allows you to specify a path to your own configuration
 * file, otherwise, inspircd.conf is read.
 */
class ConfigReader : public classbase
{
  protected:
	/** The filename of the configuration file, as set by the constructor.
	 */
	std::string fname;
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
	/** Counts the number of times a given tag appears in the config file.
	 * This method counts the number of times a tag appears in a config file, for use where
	 * there are several tags of the same kind, e.g. with opers and connect types. It can be
	 * used with the index value of ConfigReader::ReadValue to loop through all copies of a
	 * multiple instance tag.
	 */
	int Enumerate(std::string tag);
	/** Returns true if a config file is valid.
	 * This method is unimplemented and will always return true.
	 */
	bool Verify();
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
	 /** Retrieve one line from the file.
	  * This method retrieves one line from the text file. If an empty non-NULL string is returned,
	  * the index was out of bounds, or the line had no data on it.
	  */
	 bool Exists();
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
