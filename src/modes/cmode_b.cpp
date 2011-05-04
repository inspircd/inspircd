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
#include "u_listmode.h"
#include "builtin-modes.h"

ModeChannelBan::ModeChannelBan() : ListModeBase(NULL, "ban", 'b', "End of channel ban list", 367, 368, true)
{
}
