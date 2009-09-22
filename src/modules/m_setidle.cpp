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

/* $ModDesc: Allows opers to set their idle time */

/** Handle /SETIDLE
 */
class CommandSetidle : public Command
{
 public:
	CommandSetidle(Module* Creator) : Command(Creator,"SETIDLE", 1)
	{
		flags_needed = 'o'; syntax = "<duration>";
		TRANSLATE2(TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		time_t idle = ServerInstance->Duration(parameters[0]);
		if (idle < 1)
		{
			user->WriteNumeric(948, "%s :Invalid idle time.",user->nick.c_str());
			return CMD_FAILURE;
		}
		user->idle_lastmsg = (ServerInstance->Time() - idle);
		// minor tweak - we cant have signon time shorter than our idle time!
		if (user->signon > user->idle_lastmsg)
			user->signon = user->idle_lastmsg;
		ServerInstance->SNO->WriteToSnoMask('a', std::string(user->nick)+" used SETIDLE to set their idle time to "+ConvToStr(idle)+" seconds");
		user->WriteNumeric(944, "%s :Idle time set.",user->nick.c_str());

		return CMD_SUCCESS;
	}
};


class ModuleSetIdle : public Module
{
	CommandSetidle cmd;
 public:
	ModuleSetIdle(InspIRCd* Me)
		: Module(Me), cmd(this)
	{
		ServerInstance->AddCommand(&cmd);
	}

	virtual ~ModuleSetIdle()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Allows opers to set their idle time", VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleSetIdle)
