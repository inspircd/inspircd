/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modes/umode_w.h"

ModeUserWallops::ModeUserWallops(InspIRCd* Instance) : SimpleUserModeHandler(Instance, 'w')
{
}

unsigned int ModeUserWallops::GetCount()
{
	return count;
}
