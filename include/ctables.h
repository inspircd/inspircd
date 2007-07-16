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
 
#ifndef __CTABLES_H__
#define __CTABLES_H__


#include "inspircd_config.h"
#include "hash_map.h"
#include "base.h"

/* Forward declarations - required */
class userrec;
class InspIRCd;

/** Used to indicate command success codes
 */
enum CmdResult
{
	CMD_FAILURE = 0,	/* Command exists, but failed */
	CMD_SUCCESS = 1,	/* Command exists, and succeeded */
	CMD_INVALID = 2,	/* Command doesnt exist at all! */
	CMD_USER_DELETED = 3	/* User was deleted - DEPRECIATED */
};

/** For commands which should not be replicated to other
 * servers, we usually return CMD_FAILURE. this isnt readable,
 * so we define this alias for CMD_FAILURE called
 * CMD_LOCALONLY, which of course does the same thing but is
 * much more readable.
 */
#define CMD_LOCALONLY CMD_FAILURE


/** A structure that defines a command. Every command available
 * in InspIRCd must be defined as derived from command_t.
 */
class CoreExport command_t : public Extensible
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
	int min_params;
	/** used by /stats m
	 */
	long use_count;
	/** used by /stats m
 	 */
	float total_bytes;
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

	/** Create a new command.
	 * @param Instance Pointer to creator class
	 * @param cmd Command name. This must be UPPER CASE.
	 * @param flags User modes required to execute the command.
	 * For oper only commands, set this to 'o', otherwise use 0.
	 * @param minpara Minimum parameters required for the command.
	 * @param before_reg If this is set to true, the command will
	 * be allowed before the user is 'registered' (has sent USER,
	 * NICK, optionally PASS, and been resolved).
	 */
	command_t(InspIRCd* Instance, const std::string &cmd, char flags, int minpara, int before_reg = false) : ServerInstance(Instance), command(cmd), flags_needed(flags), min_params(minpara), disabled(false), works_before_reg(before_reg)
	{
		use_count = 0;
		total_bytes = 0;
		source = "<core>";
		syntax = "";
	}

	/** Handle the command from a user.
	 * @param parameters The parameters for the command.
	 * @param pcnt The number of parameters available in 'parameters'
	 * @param user The user who issued the command.
	 * @return Return CMD_SUCCESS on success, or CMD_FAILURE on failure.
	 * If the command succeeds but should remain local to this server,
	 * return CMD_LOCALONLY.
	 */
	virtual CmdResult Handle(const char** parameters, int pcnt, userrec* user) = 0;

	/** Handle an internal request from another command, the core, or a module
	 * @param Command ID
	 * @param Zero or more parameters, whos form is specified by the command ID.
	 * @return Return CMD_SUCCESS on success, or CMD_FAILURE on failure.
	 * If the command succeeds but should remain local to this server,
	 * return CMD_LOCALONLY.
	 */
	virtual CmdResult HandleInternal(const unsigned int id, const std::deque<classbase*> &params)
	{
		return CMD_INVALID;
	}

	/** Handle the command from a server.
	 * Not currently used in this version of InspIRCd.
	 * @param parameters The parameters given
	 * @param pcnt The number of parameters available
	 * @param servername The server name which issued the command
	 * @return Return CMD_SUCCESS on success, or CMD_FAILURE on failure.
	 * If the command succeeds but should remain local to this server,
	 * return CMD_LOCALONLY.
	 */
	virtual CmdResult HandleServer(const char** parameters, int pcnt, const std::string &servername)
	{
		return CMD_INVALID;
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
	virtual ~command_t() {}
};

/** A hash of commands used by the core
 */
typedef nspace::hash_map<std::string,command_t*> command_table;

#endif

