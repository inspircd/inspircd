/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
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

class InspIRCd;

/** This class handles command management and parsing.
 * It allows you to add and remove commands from the map,
 * call command handlers by name, and chop up comma seperated
 * parameters into multiple calls.
 */
class CommandParser : public classbase
{
 private:
	/** The creator of this class
	 */
	InspIRCd* ServerInstance;

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

	/** Insert the default RFC1459 commands into the command hash.
	 */
	void SetupCommandTable();

	void FindSym(void** v, void* h);

 public:
	/** Command list, a hash_map of command names to command_t*
	 */
	command_table cmdlist;

	void LoadCommand(const char* name);

	/** Default constructor.
	 * @param Instance The creator of this class
	 */
	CommandParser(InspIRCd* Instance);

	/** Calls the handler for a given command.
	 * @param commandname The command to find. This should be in uppercase.
	 * @param parameters Parameter list as an array of array of char (that's not a typo).
	 * @param pcnt The number of items in the parameters list
	 * @param user The user to call the handler on behalf of
	 * @return This method will return true if the command handler was found and called
	 */
	bool CallHandler(const std::string &commandname,const char** parameters, int pcnt, userrec *user);

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
	 * @return True if the command was added
	 */
	bool CreateCommand(command_t *f);
};

#endif
