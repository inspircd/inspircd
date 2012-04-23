/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2003, 2007 Craig Edwards <craigedwards@brainbox.cc>
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


#ifndef __CTABLES_H__
#define __CTABLES_H__

/** Used to indicate command success codes
 */
enum CmdResult
{
	CMD_FAILURE = 0,	/* Command exists, but failed */
	CMD_SUCCESS = 1,	/* Command exists, and succeeded */
	CMD_INVALID = 2		/* Command doesnt exist at all! */
};

/** Translation types for translation of parameters to UIDs.
 * This allows the core commands to not have to be aware of how UIDs
 * work (making it still possible to write other linking modules which
 * do not use UID (but why would you want to?)
 */
enum TranslateType
{
	TR_END,			/* End of known parameters, everything after this is TR_TEXT */
	TR_TEXT,		/* Raw text, leave as-is */
	TR_NICK,		/* Nickname, translate to UUID for server->server */
	TR_CUSTOM		/* Custom translation handled by EncodeParameter/DecodeParameter */
};

/** For commands which should not be replicated to other
 * servers, we usually return CMD_FAILURE. this isnt readable,
 * so we define this alias for CMD_FAILURE called
 * CMD_LOCALONLY, which of course does the same thing but is
 * much more readable.
 */
#define CMD_LOCALONLY CMD_FAILURE


/** A structure that defines a command. Every command available
 * in InspIRCd must be defined as derived from Command.
 */
class CoreExport Command : public Extensible
{
 protected:
	/** Owner/Creator object
	 */
	InspIRCd* ServerInstance;
 public:
	/** Command name
	*/
	 std::string command;

	/** User flags needed to execute the command or 0
	 */
	char flags_needed;

	/** Minimum number of parameters command takes
	*/
	const unsigned int min_params;

	/** Maximum number of parameters command takes.
	 * This is used by the command parser to join extra parameters into one last param.
	 * If not set, no munging is done to this command.
	 */
	const unsigned int max_params;

	/** used by /stats m
	 */
	long double use_count;

	/** used by /stats m
 	 */
	long double total_bytes;

	/** used for resource tracking between modules
	 */
	std::string source;

	/** True if the command is disabled to non-opers
	 */
	bool disabled;

	/** True if the command can be issued before registering
	 */
	bool works_before_reg;

	/** Syntax string for the command, displayed if non-empty string.
	 * This takes place of the text in the 'not enough parameters' numeric.
	 */
	std::string syntax;

	/** Translation type list for possible parameters, used to tokenize
	 * parameters into UIDs and SIDs etc.
	 */
	std::vector<TranslateType> translation;

	/** How many seconds worth of penalty does this command have?
	 */
	const int Penalty;

	/** Create a new command.
	 * @param Instance Pointer to creator class
	 * @param cmd Command name. This must be UPPER CASE.
	 * @param flags User mode required to execute the command. May ONLY be one mode - it's a string to give warnings if people mix params up.
	 * For oper only commands, set this to 'o', otherwise use 0.
	 * @param minpara Minimum parameters required for the command.
	 * @param maxpara Maximum number of parameters this command may have - extra parameters will be tossed into one last space-seperated param.
	 * @param before_reg If this is set to true, the command will
	 * be allowed before the user is 'registered' (has sent USER,
	 * NICK, optionally PASS, and been resolved).
	 */
	Command(InspIRCd* Instance, const std::string &cmd, const char *flags, int minpara, bool before_reg = false, int penalty = 1) : 	ServerInstance(Instance), command(cmd), flags_needed(flags ? *flags : 0), min_params(minpara), max_params(0), disabled(false), works_before_reg(before_reg), Penalty(penalty)
	{
		use_count = 0;
		total_bytes = 0;
		source = "<core>";
		syntax = "";
		translation.clear();
	}

	Command(InspIRCd* Instance, const std::string &cmd, const char *flags, int minpara, int maxpara, bool before_reg = false, int penalty = 1) : 	ServerInstance(Instance), command(cmd), flags_needed(flags ? *flags : 0), min_params(minpara), max_params(maxpara), disabled(false), works_before_reg(before_reg), Penalty(penalty)
	{
		use_count = 0;
		total_bytes = 0;
		source = "<core>";
		syntax = "";
		translation.clear();
	}

	/** Handle the command from a user.
	 * @param parameters The parameters for the command.
	 * @param user The user who issued the command.
	 * @return Return CMD_SUCCESS on success, or CMD_FAILURE on failure.
	 * If the command succeeds but should remain local to this server,
	 * return CMD_LOCALONLY.
	 */
	virtual CmdResult Handle(const std::vector<std::string>& parameters, User* user) = 0;

	/** Handle an internal request from another command, the core, or a module
	 * @param Command ID
	 * @param Zero or more parameters, whos form is specified by the command ID.
	 * @return Return CMD_SUCCESS on success, or CMD_FAILURE on failure.
	 * If the command succeeds but should remain local to this server,
	 * return CMD_LOCALONLY.
	 */
	virtual CmdResult HandleInternal(const unsigned int /* id */, const std::deque<classbase*>& /* params */)
	{
		return CMD_INVALID;
	}

	/** Handle the command from a server.
	 * Not currently used in this version of InspIRCd.
	 * @param parameters The parameters given
	 * @param servername The server name which issued the command
	 * @return Return CMD_SUCCESS on success, or CMD_FAILURE on failure.
	 * If the command succeeds but should remain local to this server,
	 * return CMD_LOCALONLY.
	 */
	virtual CmdResult HandleServer(const std::vector<std::string>& /* parameters */, const std::string& /* servername */)
	{
		return CMD_INVALID;
	}

	/** Encode a parameter for server->server transmission.
	 * Used for parameters for which the translation type is TR_CUSTOM.
	 * @param parameter The parameter to encode. Can be modified in place.
	 * @param index The parameter index (0 == first parameter).
	 */
	virtual void EncodeParameter(std::string& parameter, int index)
	{
	}

	/** Decode a parameter from server->server transmission.
	 * Not currently used in this version of InspIRCd.
	 * Used for parameters for which the translation type is TR_CUSTOM.
	 * @param parameter The parameter to decode. Can be modified in place.
	 * @param index The parameter index (0 == first parameter).
	 */
	virtual void DecodeParameter(std::string& parameter, int index)
	{
	}

	/** Disable or enable this command.
	 * @param setting True to disable the command.
	 */
	void Disable(bool setting)
	{
		disabled = setting;
	}

	/** Obtain this command's disable state.
	 * @return true if the command is currently disabled
	 * (disabled commands can be used only by operators)
	 */
	bool IsDisabled()
	{
		return disabled;
	}

	/** @return true if the command works before registration.
	 */
	bool WorksBeforeReg()
	{
		return works_before_reg;
	}

	/** Standard constructor gubbins
	 */
	virtual ~Command()
	{
		syntax.clear();
	}
};

/** A hash of commands used by the core
 */
typedef nspace::hash_map<std::string,Command*> Commandtable;

/** Shortcut macros for defining translation lists
 */
#define TRANSLATE1(x1)	translation.push_back(x1);
#define TRANSLATE2(x1,x2)  translation.push_back(x1);translation.push_back(x2);
#define TRANSLATE3(x1,x2,x3)  translation.push_back(x1);translation.push_back(x2);translation.push_back(x3);
#define TRANSLATE4(x1,x2,x3,x4)  translation.push_back(x1);translation.push_back(x2);translation.push_back(x3);translation.push_back(x4);
#define TRANSLATE5(x1,x2,x3,x4,x5)  translation.push_back(x1);translation.push_back(x2);translation.push_back(x3);translation.push_back(x4);\
	translation.push_back(x5);
#define TRANSLATE6(x1,x2,x3,x4,x5,x6)  translation.push_back(x1);translation.push_back(x2);translation.push_back(x3);translation.push_back(x4);\
	translation.push_back(x5);translation.push_back(x6);
#define TRANSLATE7(x1,x2,x3,x4,x5,x6,x7)  translation.push_back(x1);translation.push_back(x2);translation.push_back(x3);translation.push_back(x4);\
	translation.push_back(x5);translation.push_back(x6);translation.push_back(x7);
#define TRANSLATE8(x1,x2,x3,x4,x5,x6,x7,x8)  translation.push_back(x1);translation.push_back(x2);translation.push_back(x3);translation.push_back(x4);\
	translation.push_back(x5);translation.push_back(x6);translation.push_back(x7);translation.push_back(x8);

#endif
