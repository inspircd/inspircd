/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	  the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"

ModResult ModuleSpanningTree::OnPreCommand(std::string &command, std::vector<std::string>& parameters, LocalUser *user, bool validated, const std::string &original_line)
{
	/* If the command doesnt appear to be valid, we dont want to mess with it. */
	if (!validated)
		return MOD_RES_PASSTHRU;

	if (command == "CONNECT")
	{
		return this->HandleConnect(parameters,user);
	}
	else if (command == "SQUIT")
	{
		return this->HandleSquit(parameters,user);
	}
	else if (command == "MAP")
	{
		return this->HandleMap(parameters,user) ? MOD_RES_DENY : MOD_RES_PASSTHRU;
	}
	else if (command == "LINKS")
	{
		this->HandleLinks(parameters,user);
		return MOD_RES_DENY;
	}
	else if ((command == "VERSION") && (parameters.size() > 0))
	{
		this->HandleVersion(parameters,user);
		return MOD_RES_DENY;
	}
	return MOD_RES_PASSTHRU;
}

