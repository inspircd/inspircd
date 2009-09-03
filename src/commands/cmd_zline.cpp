/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "xline.h"
/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *      the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __CMD_ZLINE_H__
#define __CMD_ZLINE_H__

// include the common header files

#include "users.h"
#include "channels.h"

/** Handle /ZLINE. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandZline : public Command
{
 public:
	/** Constructor for zline.
	 */
	CommandZline (InspIRCd* Instance, Module* parent) : Command(Instance,parent,"ZLINE","o",1,3,false,0) { syntax = "<ipmask> [<duration> :<reason>]"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

#endif




CmdResult CommandZline::Handle (const std::vector<std::string>& parameters, User *user)
{
	std::string target = parameters[0];

	if (parameters.size() >= 3)
	{
		if (target.find('!') != std::string::npos)
		{
			user->WriteServ("NOTICE %s :*** You cannot include a nickname in a zline, a zline must ban only an IP mask",user->nick.c_str());
			return CMD_FAILURE;
		}

		User *u = ServerInstance->FindNick(target.c_str());

		if (u)
		{
			target = u->GetIPString();
		}

		const char* ipaddr = target.c_str();

		if (strchr(ipaddr,'@'))
		{
			while (*ipaddr != '@')
				ipaddr++;
			ipaddr++;
		}

		if (ServerInstance->IPMatchesEveryone(ipaddr,user))
			return CMD_FAILURE;

		long duration = ServerInstance->Duration(parameters[1].c_str());

		ZLine* zl = new ZLine(ServerInstance, ServerInstance->Time(), duration, user->nick.c_str(), parameters[2].c_str(), ipaddr);
		if (ServerInstance->XLines->AddLine(zl,user))
		{
			if (!duration)
			{
				ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent Z-line for %s: %s", user->nick.c_str(), ipaddr, parameters[2].c_str());
			}
			else
			{
				time_t c_requires_crap = duration + ServerInstance->Time();
				ServerInstance->SNO->WriteToSnoMask('x',"%s added timed Z-line for %s, expires on %s: %s",user->nick.c_str(),ipaddr,
						ServerInstance->TimeString(c_requires_crap).c_str(), parameters[2].c_str());
			}
			ServerInstance->XLines->ApplyLines();
		}
		else
		{
			delete zl;
			user->WriteServ("NOTICE %s :*** Z-Line for %s already exists",user->nick.c_str(),ipaddr);
		}
	}
	else
	{
		if (ServerInstance->XLines->DelLine(target.c_str(),"Z",user))
		{
			ServerInstance->SNO->WriteToSnoMask('x',"%s Removed Z-line on %s.",user->nick.c_str(),target.c_str());
		}
		else
		{
			user->WriteServ("NOTICE %s :*** Z-Line %s not found in list, try /stats Z.",user->nick.c_str(),target.c_str());
			return CMD_FAILURE;
		}
	}

	return CMD_SUCCESS;
}

COMMAND_INIT(CommandZline)
