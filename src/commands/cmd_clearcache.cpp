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

#ifndef __CMD_ADMIN_H__
#define __CMD_ADMIN_H__

#include "users.h"
#include "channels.h"
#include "ctables.h"

/** Handle /ADMIN. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandClearcache : public Command
{
 public:
	/** Constructor for clearcache.
	 */
	CommandClearcache (InspIRCd* Instance, Module* parent) : Command(Instance,parent,"CLEARCACHE","o",0) { }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

#endif


/** Handle /CLEARCACHE
 */
CmdResult CommandClearcache::Handle (const std::vector<std::string>& parameters, User *user)
{
	int n = ServerInstance->Res->ClearCache();
	user->WriteServ("NOTICE %s :*** Cleared DNS cache of %d items.", user->nick.c_str(), n);
	return CMD_SUCCESS;
}

COMMAND_INIT(CommandClearcache)
