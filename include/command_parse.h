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


#pragma once

/** This class handles command management and parsing.
 * It allows you to add and remove commands from the map,
 * call command handlers by name, and chop up comma seperated
 * parameters into multiple calls.
 */
class CoreExport CommandParser
{
 public:
 	typedef TR1NS::unordered_map<std::string, Command*> CommandMap;

 private:
	/** Process a command from a user.
	 * @param user The user to parse the command for
	 * @param cmd The command string to process
	 */
	void ProcessCommand(LocalUser* user, std::string& cmd);

	/** Command list, a hash_map of command names to Command*
	 */
	CommandMap cmdlist;

 public:
	/** Default constructor.
	 */
	CommandParser();

	/** Get a command name -> Command* map containing all client to server commands
	 * @return A map of command handlers keyed by command names
	 */
	const CommandMap& GetCommands() const { return cmdlist; }

	/** Calls the handler for a given command.
	 * @param commandname The command to find. This should be in uppercase.
	 * @param parameters Parameter list
	 * @param user The user to call the handler on behalf of
	 * @param cmd If non-NULL and the command was executed it is set to the command handler,
	 * otherwise it isn't written to.
	 * @return This method will return CMD_SUCCESS if the command handler was found and called,
	 * and the command completeld successfully. It will return CMD_FAILURE if the command handler was found
	 * and called, but the command did not complete successfully, and it will return CMD_INVALID if the
	 * command simply did not exist at all or the wrong number of parameters were given, or the user
	 * was not privilaged enough to execute the command.
	 */
	CmdResult CallHandler(const std::string& commandname, const std::vector<std::string>& parameters, User* user, Command** cmd = NULL);

	/** Get the handler function for a command.
	 * @param commandname The command required. Always use uppercase for this parameter.
	 * @return a pointer to the command handler, or NULL
	 */
	Command* GetHandler(const std::string &commandname);

	/** LoopCall is used to call a command handler repeatedly based on the contents of a comma seperated list.
	 * There are two ways to call this method, either with one potential list or with two potential lists.
	 * We need to handle two potential lists for JOIN, because a JOIN may contain two lists of items at once:
	 * the channel names and their keys as follows:
	 *
	 * JOIN \#chan1,\#chan2,\#chan3 key1,,key3
	 *
	 * Therefore, we need to deal with both lists concurrently. If there are two lists then the method reads
	 * them both together until the first runs out of tokens.
	 * With one list it is much simpler, and is used in NAMES, WHOIS, PRIVMSG etc.
	 *
	 * If there is only one list and there are duplicates in it, then the command handler is only called for
	 * unique items. Entries are compared using "irc comparison".
	 * If the usemax parameter is true (the default) the function only parses until it reaches
	 * ServerInstance->Config->MaxTargets number of targets, to stop abuse via spam.
	 *
	 * The OnPostCommand hook is executed for each item after it has been processed by the handler, with the
	 * original line parameter being empty (to indicate that the command in that form was created by this function).
	 * This only applies if the user executing the command is local.
	 *
	 * If there are two lists and the second list runs out of tokens before the first list then parameters[extra]
	 * will be an EMPTY string when Handle() is called for the remaining tokens in the first list, even if it is
	 * in the middle of parameters[]! Moreover, empty tokens in the second list are allowed, and those will also
	 * result in the appropiate entry being empty in parameters[].
	 * This is different than what command handlers usually expect; the command parser only allows an empty param
	 * as the last item in the vector.
	 *
	 * @param user The user who sent the command
	 * @param handler The command handler to call for each parameter in the list
	 * @param parameters Parameter list as a vector of strings
	 * @param splithere The first parameter index to split as a comma seperated list
	 * @param extra The second parameter index to split as a comma seperated list, or -1 (the default) if there is only one list
	 * @param usemax True to limit the command to MaxTargets targets (default), or false to process all tokens
	 * @return This function returns true when it identified a list in the given parameter and finished calling the
	 * command handler for each entry on the list. When this occurs, the caller should return without doing anything,
	 * otherwise it should continue into its main section of code.
	 */
	static bool LoopCall(User* user, Command* handler, const std::vector<std::string>& parameters, unsigned int splithere, int extra = -1, bool usemax = true);

	/** Take a raw input buffer from a recvq, and process it on behalf of a user.
	 * @param buffer The buffer line to process
	 * @param user The user to whom this line belongs
	 */
	void ProcessBuffer(std::string &buffer,LocalUser *user);

	/** Add a new command to the commands hash
	 * @param f The new Command to add to the list
	 * @return True if the command was added
	 */
	bool AddCommand(Command *f);

	/** Removes a command.
	 */
	void RemoveCommand(Command* x);

	/** Translate a single item based on the TranslationType given.
	 * @param to The translation type to use for the process
	 * @param item The input string
	 * @param dest The output string. The translation result will be appended to this string
	 * @param custom_translator Used to translate the parameter if the translation type is TR_CUSTOM, if NULL, TR_CUSTOM will act like TR_TEXT
	 * @param paramnumber The index of the parameter we are translating.
	 */
	static void TranslateSingleParam(TranslateType to, const std::string& item, std::string& dest, CommandBase* custom_translator = NULL, unsigned int paramnumber = 0);

	/** Translate nicknames in a list of strings into UIDs, based on the TranslateTypes given.
	 * @param to The translation types to use for the process. If this list is too short, TR_TEXT is assumed for the rest.
	 * @param source The strings to translate
	 * @param prefix_final True if the final source argument should have a colon prepended (if it could contain a space)
	 * @param custom_translator Used to translate the parameter if the translation type is TR_CUSTOM, if NULL, TR_CUSTOM will act like TR_TEXT
	 * @return dest The output string
	 */
	static std::string TranslateUIDs(const std::vector<TranslateType>& to, const std::vector<std::string>& source, bool prefix_final = false, CommandBase* custom_translator = NULL);
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
