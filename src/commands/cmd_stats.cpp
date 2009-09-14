/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#ifndef WIN32
	#include <sys/resource.h>
	/* This is just to be completely certain that the change which fixed getrusage on RH7 doesn't break anything else -- Om */
	#ifndef RUSAGE_SELF
	#define RUSAGE_SELF 0
	#endif
#else
	#include <psapi.h>
	#include "inspircd_win32wrapper.h"
	#pragma comment(lib, "psapi.lib")
#endif

#include "xline.h"

/** Handle /STATS. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandStats : public Command
{
 public:
	/** Constructor for stats.
	 */
	CommandStats ( Module* parent) : Command(parent,"STATS",1,2) { syntax = "<stats-symbol> [<servername>]"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

CmdResult CommandStats::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (IS_LOCAL(user))
	{
		string_list values;
		if (parameters[0].empty())
		{
			user->WriteNumeric(ERR_NEEDMOREPARAMS, "%s STATS :Not enough parameters.", user->nick.c_str());
			return CMD_FAILURE;
		}
		char search = parameters[0][0];
		ServerInstance->DoStats(search, user, values);
		for (size_t i = 0; i < values.size(); i++)
			user->Write(":%s", values[i].c_str());
	}

	return CMD_SUCCESS;
}

COMMAND_INIT(CommandStats)
