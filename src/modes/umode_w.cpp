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
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modes/umode_w.h"

ModeUserWallops::ModeUserWallops(InspIRCd* Instance) : SimpleUserModeHandler(Instance, NULL, 'w')
{
}

unsigned int ModeUserWallops::GetCount()
{
	return count;
}
