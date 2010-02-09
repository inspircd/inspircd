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
#include "modes/cmode_b.h"

ModeChannelBan::ModeChannelBan() : ListModeBase(NULL, "ban", 'b', "End of channel ban list", 367, 368, true)
{
}
