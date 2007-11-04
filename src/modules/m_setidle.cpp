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

#include "inspircd.h"

/* $ModDesc: Allows opers to set their idle time */

/** Handle /SETIDLE
 */
class CommandSetidle : public Command
{
 public:
	CommandSetidle (InspIRCd* Instance) : Command(Instance,"SETIDLE", 'o', 1)
	{
		this->source = "m_setidle.so";
		syntax = "<duration>";
		TRANSLATE2(TR_TEXT, TR_END);
	}

	CmdResult Handle (const char** parameters, int pcnt, User *user)
	{
		time_t idle = ServerInstance->Duration(parameters[0]);
		if (idle < 1)
		{
			user->WriteServ("948 %s :Invalid idle time.",user->nick);
			return CMD_FAILURE;
		}
		user->idle_lastmsg = (ServerInstance->Time() - idle);
		// minor tweak - we cant have signon time shorter than our idle time!
		if (user->signon > user->idle_lastmsg)
			user->signon = user->idle_lastmsg;
		ServerInstance->WriteOpers(std::string(user->nick)+" used SETIDLE to set their idle time to "+ConvToStr(idle)+" seconds");
		user->WriteServ("944 %s :Idle time set.",user->nick);

		return CMD_LOCALONLY;
	}
};


class ModuleSetIdle : public Module
{
	CommandSetidle*	mycommand;
 public:
	ModuleSetIdle(InspIRCd* Me)
		: Module(Me)
	{
		
		mycommand = new CommandSetidle(ServerInstance);
		ServerInstance->AddCommand(mycommand);

	}
	
	virtual ~ModuleSetIdle()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleSetIdle)
