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
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides support for an SAQUIT command, exits user with a reason */

/** Handle /SAQUIT
 */
class cmd_saquit : public command_t
{
 public:
 cmd_saquit (InspIRCd* Instance) : command_t(Instance,"SAQUIT",'o',2)
	{
		this->source = "m_saquit.so";
		syntax = "<nick> <reason>";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		userrec* dest = ServerInstance->FindNick(parameters[0]);
		if (dest)
		{
			if (ServerInstance->ULine(dest->server))
			{
				user->WriteServ("990 %s :Cannot use an SA command on a u-lined client",user->nick);
				return CMD_FAILURE;
			}
			irc::stringjoiner reason_join(" ", parameters, 1, pcnt - 1);
			std::string line = reason_join.GetJoined();

			ServerInstance->WriteOpers("*** "+std::string(user->nick)+" used SAQUIT to make "+std::string(dest->nick)+" quit with a reason of "+line);
			userrec::QuitUser(ServerInstance, dest, line);

			return CMD_SUCCESS;
		}
		else
		{
			user->WriteServ("NOTICE %s :*** Invalid nickname '%s'", user->nick, parameters[0]);
		}

		return CMD_FAILURE;
	}
};

class ModuleSaquit : public Module
{
	cmd_saquit*	mycommand;
 public:
	ModuleSaquit(InspIRCd* Me)
		: Module(Me)
	{
		
		mycommand = new cmd_saquit(ServerInstance);
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~ModuleSaquit()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
	
};

MODULE_INIT(ModuleSaquit)
