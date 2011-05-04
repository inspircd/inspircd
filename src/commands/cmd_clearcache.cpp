/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

#include "dns.h"
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
	CommandClearcache ( Module* parent) : Command(parent,"CLEARCACHE",0) { flags_needed = 'o'; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

/** Handle /CLEARCACHE
 */
CmdResult CommandClearcache::Handle (const std::vector<std::string>& parameters, User *user)
{
	int n = ServerInstance->Res->ClearCache();
	user->WriteServ("NOTICE %s :*** Cleared DNS cache of %d items.", user->nick.c_str(), n);
	return CMD_SUCCESS;
}

COMMAND_INIT(CommandClearcache)
