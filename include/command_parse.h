/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2007 Craig Edwards <craigedwards@brainbox.cc>
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


#ifndef COMMAND_PARSE_H
#define COMMAND_PARSE_H

/** This class handles command management and parsing.
 * It allows you to add and remove commands from the map,
 * call command handlers by name, and chop up comma seperated
 * parameters into multiple calls.
 */
class CoreExport CommandParser
{
 private:
	/** Process a parameter string into a list of items
	 * @param command_p The output list of items
	 * @param parameters The input string
	 * @return The number of parameters parsed into command_p
	 */
	int ProcessParameters(std::vector<std::string>& command_p, char* parameters);

	/** Process a command from a user.
	 * @param user The user to parse the command for
	 * @param cmd The command string to process
	 */
	bool ProcessCommand(LocalUser *user, std::string &cmd);

 public:
	/** Command list, a hash_map of command names to Command*
	 */
	Commandtable cmdlist;

	/** Default constructor.
	 */
	CommandParser();

	/** Calls the handler for a given command.
	 * @param commandname The command to find. This should be in uppercase.
	 * @param parameters Parameter list
	 * @param user The user to call the handler on behalf of
	 * @return This method will return CMD_SUCCESS if the command handler was found and called,
	 * and the command completeld successfully. It will return CMD_FAILURE if the command handler was found
	 * and called, but the command did not complete successfully, and it will return CMD_INVALID if the
	 * command simply did not exist at all or the wrong number of parameters were given, or the user
	 * was not privilaged enough to execute the command.
	 */
	CmdResult CallHandler(const std::string &commandname, const std::vector<std::string>& parameters, User *user);

	/** Get the handler function for a command.
	 * @param commandname The command required. Always use uppercase for this parameter.
	 * @return a pointer to the command handler, or NULL
	 */
	Command* GetHandler(const std::string &commandname);

	/** This function returns true if a command is valid with the given number of parameters and user.
	 * @param commandname The command name to check
	 * @param pcnt The parameter count
	 * @param user The user to check against
	 * @return If the user given has permission to execute the command, and the parameter count is
	 * equal to or greater than the minimum number of parameters to the given command, then this
	 * function will return true, otherwise it will return false.
	 */
	bool IsValidCommand(const std::string &commandname, unsigned int pcnt, User * user);

	/** LoopCall is used to call a command classes handler repeatedly based on the contents of a comma seperated list.
	 * There are two overriden versions of this method, one of which takes two potential lists and the other takes one.
	 * We need a version which takes two potential lists for JOIN, because a JOIN may contain two lists of items at once,
	 * the channel names and their keys as follows:
	 *
	 * JOIN \#chan1,\#chan2,\#chan3 key1,,key3
	 *
	 * Therefore, we need to deal with both lists concurrently. The first instance of this method does that by creating
	 * two instances of irc::commasepstream and reading them both together until the first runs out of tokens.
	 * The second version is much simpler and just has the one stream to read, and is used in NAMES, WHOIS, PRIVMSG etc.
	 * Both will only parse until they reach ServerInstance->Config->MaxTargets number of targets, to stop abuse via spam.
	 *
	 * @param user The user who sent the command
	 * @param CommandObj the command object to call for each parameter in the list
	 * @param parameters Parameter list as an array of array of char (that's not a typo).
	 * @param splithere The first parameter index to split as a comma seperated list
	 * @param extra The second parameter index to split as a comma seperated list
	 * @param usemax Limit the command to MaxTargets targets
	 * @return This function will return 1 when there are no more parameters to process. When this occurs, its
	 * caller should return without doing anything, otherwise it should continue into its main section of code.
	 */
	int LoopCall(User* user, Command* CommandObj, const std::vector<std::string>& parameters, unsigned int splithere, int extra = -1, bool usemax = true);

	/** Take a raw input buffer from a recvq, and process it on behalf of a user.
	 * @param buffer The buffer line to process
	 * @param user The user to whom this line belongs
	 */
	bool ProcessBuffer(std::string &buffer,LocalUser *user);

	/** Add a new command to the commands hash
	 * @param f The new Command to add to the list
	 * @return True if the command was added
	 */
	bool AddCommand(Command *f);

	/** Removes a command.
	 */
	void RemoveCommand(Command* x);

	/** Translate nicknames in a string into UIDs, based on the TranslationType given.
	 * @param to The translation type to use for the process.
	 * @param source The input string
	 * @param dest The output string, it is safe to pass source and dest as the same variable only for translation type TR_TEXT.
	 * @return returns the number of substitutions made. Will always be 0 or 1
	 */
	int TranslateUIDs(TranslateType to, const std::string &source, std::string &dest);

	/** Translate nicknames in a list of strings into UIDs, based on the TranslateTypes given.
	 * @param to The translation types to use for the process. If this list is too short, TR_TEXT is assumed for the rest.
	 * @param source The strings to translate
	 * @param dest The output string
	 * @param prefix_final True if the final source argument should have a colon prepended (if it could contain a space)
	 * @param custom_translator Used to translate the parameter if the TR_CUSTOM type is found in to
	 * @return returns the number of substitutions made.
	 */
	int TranslateUIDs(const std::vector<TranslateType> to, const std::vector<std::string> &source, std::string &dest, bool prefix_final = false, Command* custom_translator = NULL);
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
	604800, 1, 31557600, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 86400, 1, 1, 1, 3600, 1, 1, 1, 1, 60,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 604800, 1, 31557600,
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
