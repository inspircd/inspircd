/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"

bool TreeSocket::ServerVersion(const std::string &prefix, parameterlist &params)
{
	if (params.size() < 1)
		return true;

	TreeServer* ServerSource = Utils->FindServer(prefix);

	if (ServerSource)
		ServerSource->VersionString = params[0];
	params[0] = ":" + params[0];
	Utils->DoOneToAllButSender(prefix,"VERSION",params,prefix);
	return true;
}

