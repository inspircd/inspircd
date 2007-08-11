/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __COMMAND_PARSE_H
#define __COMMAND_PARSE_H

#include <string>
#include "users.h"
#include "ctables.h"
#include "typedefs.h"

/** Required forward declaration
 */
class InspIRCd;

/** A list of dll/so files containing the command handlers for the core
 */
typedef std::map<std::string, void*> SharedObjectList;

/** This class handles command management and parsing.
 * It allows you to add and remove commands from the map,
 * call command handlers by name, and chop up comma seperated
 * parameters into multiple calls.
 */
class CoreExport CommandParser : public classbase
{
 private:
	/** The creator of this class
	 */
	InspIRCd* ServerInstance;

	/** Parameter buffer
	 */
	std::vector<std::string> para;

	/** Process a parameter string into a list of items
	 * @param command_p The output list of items
	 * @param parameters The input string
	 * @return The number of parameters parsed into command_p
	 */
	int ProcessParameters(char **command_p,char *parameters);

	/** Process a command from a user.
	 * @param user The user to parse the command for
	 * @param cmd The command string to process
	 */
	void ProcessCommand(userrec *user, std::string &cmd);

	/** Finds the init_command symbol in a .so file
	 * @param v A function pointer to be initialized
	 * @param h A valid shared object handle
	 * @return True if the symbol could be found
	 */
	bool FindSym(void** v, void* h);

	/** A list of core-implemented modes and their shared object handles
	 */
	SharedObjectList RFCCommands;

	/** Load a command from a shared object on disk.
	 * @param name The shared object to load (without path)
	 * @return NULL on success, pointer to dlerrr() error message on failure
	 */
	const char* LoadCommand(const char* name);

	/** Removes a command if the sources match. Used as a helper for
	 *  safe hash_map delete while iter in RemoveCommands(const char* source).
	 */
	void RemoveCommand(nspace::hash_map<std::string,command_t*>::iterator safei, const char* source);


 public:
	/** Command list, a hash_map of command names to command_t*
	 */
	command_table cmdlist;

	/** Reload a core command.
	 * This will only reload commands implemented by the core,
	 * to reload a modular command, you must reload that module.
	 * @param cmd The command to reload. This will cause the shared
	 * object which implements this command to be closed, and then reloaded.
	 * @return True if the command was reloaded, false if it could not be found
	 * or another error occured
	 */
	bool ReloadCommand(const char* cmd, userrec* user);

	/** Default constructor.
	 * @param Instance The creator of this class
	 */
	CommandParser(InspIRCd* Instance);

	/** Calls the handler for a given command.
	 * @param commandname The command to find. This should be in uppercase.
	 * @param parameters Parameter list as an array of array of char (that's not a typo).
	 * @param pcnt The number of items in the parameters list
	 * @param user The user to call the handler on behalf of
	 * @return This method will return CMD_SUCCESS if the command handler was found and called,
	 * and the command completeld successfully. It will return CMD_FAILURE if the command handler was found
	 * and called, but the command did not complete successfully, and it will return CMD_INVALID if the
	 * command simply did not exist at all or the wrong number of parameters were given, or the user
	 * was not privilaged enough to execute the command.
	 */
	CmdResult CallHandler(const std::string &commandname,const char** parameters, int pcnt, userrec *user);

	/** Get the handler function for a command.
	 * @param commandname The command required. Always use uppercase for this parameter.
	 * @return a pointer to the command handler, or NULL
	 */
	command_t* GetHandler(const std::string &commandname);

	/** This function returns true if a command is valid with the given number of parameters and user.
	 * @param commandname The command name to check
	 * @param pcnt The parameter count
	 * @param user The user to check against
	 * @return If the user given has permission to execute the command, and the parameter count is
	 * equal to or greater than the minimum number of parameters to the given command, then this
	 * function will return true, otherwise it will return false.
	 */
	bool IsValidCommand(const std::string &commandname, int pcnt, userrec * user);
	
	/** LoopCall is used to call a command classes handler repeatedly based on the contents of a comma seperated list.
	 * There are two overriden versions of this method, one of which takes two potential lists and the other takes one.
	 * We need a version which takes two potential lists for JOIN, because a JOIN may contain two lists of items at once,
	 * the channel names and their keys as follows:
	 *
	 * JOIN #chan1,#chan2,#chan3 key1,,key3
	 *
	 * Therefore, we need to deal with both lists concurrently. The first instance of this method does that by creating
	 * two instances of irc::commasepstream and reading them both together until the first runs out of tokens.
	 * The second version is much simpler and just has the one stream to read, and is used in NAMES, WHOIS, PRIVMSG etc.
	 * Both will only parse until they reach ServerInstance->Config->MaxTargets number of targets, to stop abuse via spam.
	 *
	 * @param user The user who sent the command
	 * @param CommandObj the command object to call for each parameter in the list
	 * @param parameters Parameter list as an array of array of char (that's not a typo).
	 * @param The number of items in the parameters list
	 * @param splithere The first parameter index to split as a comma seperated list
	 * @param extra The second parameter index to split as a comma seperated list
	 * @return This function will return 1 when there are no more parameters to process. When this occurs, its
	 * caller should return without doing anything, otherwise it should continue into its main section of code.
	 */
	int LoopCall(userrec* user, command_t* CommandObj, const char** parameters, int pcnt, unsigned int splithere, unsigned int extra);

	/** LoopCall is used to call a command classes handler repeatedly based on the contents of a comma seperated list.
	 * There are two overriden versions of this method, one of which takes two potential lists and the other takes one.
	 * We need a version which takes two potential lists for JOIN, because a JOIN may contain two lists of items at once,
	 * the channel names and their keys as follows:
	 *
	 * JOIN #chan1,#chan2,#chan3 key1,,key3
	 *
	 * Therefore, we need to deal with both lists concurrently. The first instance of this method does that by creating
	 * two instances of irc::commasepstream and reading them both together until the first runs out of tokens.
	 * The second version is much simpler and just has the one stream to read, and is used in NAMES, WHOIS, PRIVMSG etc.
	 * Both will only parse until they reach ServerInstance->Config->MaxTargets number of targets, to stop abuse via spam.
	 *
	 * @param user The user who sent the command
	 * @param CommandObj the command object to call for each parameter in the list
	 * @param parameters Parameter list as an array of array of char (that's not a typo).
	 * @param The number of items in the parameters list
	 * @param splithere The first parameter index to split as a comma seperated list
	 * @param extra The second parameter index to split as a comma seperated list
	 * @return This function will return 1 when there are no more parameters to process. When this occurs, its
	 * caller should return without doing anything, otherwise it should continue into its main section of code.
	 */
	int LoopCall(userrec* user, command_t* CommandObj, const char** parameters, int pcnt, unsigned int splithere);

	/** Take a raw input buffer from a recvq, and process it on behalf of a user.
	 * @param buffer The buffer line to process
	 * @param user The user to whom this line belongs
	 */
	void ProcessBuffer(std::string &buffer,userrec *user);

	/** Remove all commands relating to module 'source'.
	 * @param source A module name which has introduced new commands
	 * @return True This function returns true if commands were removed
	 */
	bool RemoveCommands(const char* source);

	/** Add a new command to the commands hash
	 * @param f The new command_t to add to the list
	 * @param so_handle The handle to the shared object where the command can be found.
	 * Only core commands loaded via cmd_*.so files should set this parameter to anything
	 * meaningful. Module authors should leave this parameter at its default of NULL.
	 * @return True if the command was added
	 */
	bool CreateCommand(command_t *f, void* so_handle = NULL);

	/** Insert the default RFC1459 commands into the command hash.
	 * Ignore any already loaded commands.
	 * @param user User to spool errors to, or if NULL, when an error occurs spool the errors to
	 * stdout then exit with EXIT_STATUS_HANDLER.
	 */
	void SetupCommandTable(userrec* user);
};

/** Command handler class for the RELOAD command.
 * A command cant really reload itself, so this has to be in here.
 */
class cmd_reload : public command_t
{
 public:
	/** Standard constructor
	 */
	cmd_reload (InspIRCd* Instance) : command_t(Instance,"RELOAD",'o',1) { syntax = "<core-command>"; }
	/** Handle RELOAD
	 */
	CmdResult Handle(const char** parameters, int pcnt, userrec *user);
};

/** A lookup table of values for multiplier characters used by
 * InspIRCd::Duration(). In this lookup table, the indexes for
 * the ascii values 'm' and 'M' have the value '60', the indexes
 * for the ascii values 'D' and 'd' have a value of '86400', etc.
 */
const int duration_multi[] =
{
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 86400, 1, 1, 1, 3600,
	1, 1, 1, 1, 60, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	604800, 1, 31536000, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 86400, 1, 1, 1, 3600, 1, 1, 1, 1, 60,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 604800, 1, 31536000,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

#endif

