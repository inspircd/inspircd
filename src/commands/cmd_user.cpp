/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/** Handle /USER. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandUser : public SplitCommand
{
 public:
	/** Constructor for user.
	 */
	CommandUser ( Module* parent) : SplitCommand(parent,"USER",4,4) { works_before_reg = true; Penalty = 0; syntax = "<username> <localhost> <remotehost> <GECOS>"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult HandleLocal(const std::vector<std::string>& parameters, LocalUser *user);
};

CmdResult CommandUser::HandleLocal(const std::vector<std::string>& parameters, LocalUser *user)
{
	/* A user may only send the USER command once */
	if (!(user->registered & REG_USER))
	{
		if (!ServerInstance->IsIdent(parameters[0].c_str()))
		{
			/*
			 * RFC says we must use this numeric, so we do. Let's make it a little more nub friendly though. :)
			 *  -- Craig, and then w00t.
			 */
			user->WriteNumeric(461, "%s USER :Your username is not valid",user->nick.c_str());
			return CMD_FAILURE;
		}
		else
		{
			/*
			 * The ident field is IDENTMAX+2 in size to account for +1 for the optional
			 * ~ character, and +1 for null termination, therefore we can safely use up to
			 * IDENTMAX here.
			 */
			user->ChangeIdent(parameters[0].c_str());
			user->fullname.assign(parameters[3].empty() ? std::string("No info") : parameters[3], 0, ServerInstance->Config->Limits.MaxGecos);
			user->registered = (user->registered | REG_USER);
		}
	}
	else
	{
		user->WriteNumeric(462, "%s :You may not reregister", user->nick.c_str());
		return CMD_FAILURE;
	}

	/* parameters 2 and 3 are local and remote hosts, and are ignored */
	if (user->registered == REG_NICKUSER)
	{
		ModResult MOD_RESULT;

		/* user is registered now, bit 0 = USER command, bit 1 = sent a NICK command */
		FIRST_MOD_RESULT(OnUserRegister, MOD_RESULT, (user));
		if (MOD_RESULT == MOD_RES_DENY)
			return CMD_FAILURE;

	}

	return CMD_SUCCESS;
}

COMMAND_INIT(CommandUser)
