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
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modes/umode_n.h"

ModeUserServerNoticeMask::ModeUserServerNoticeMask(InspIRCd* Instance) : ModeHandler(Instance, 'n', 1, 0, false, MODETYPE_USER, true)
{
}

ModeAction ModeUserServerNoticeMask::OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
{
	/* Only opers can change other users modes */
	if ((source != dest) && (!*source->oper))
		return MODEACTION_DENY;

	/* Set the bitfields */
	if (adding)
	{
		/* Fix for bug #310 reported by Smartys */
		if (!dest->modes[UM_SNOMASK])
			memset(dest->snomasks, 0, sizeof(dest->snomasks));

		parameter = dest->ProcessNoticeMasks(parameter.c_str());
		dest->modes[UM_SNOMASK] = true;
		if (!dest->modes[UM_SERVERNOTICE])
		{
			const char* newmodes[] = { dest->nick, "+s" };
			ServerInstance->Modes->Process(newmodes, 2, source, true);
		}
		return MODEACTION_ALLOW;
	}
	else
	{
		if (dest->modes[UM_SNOMASK] != false)
		{
			dest->modes[UM_SNOMASK] = false;
			return MODEACTION_ALLOW;
		}
	}

	/* Allow the change */
	return MODEACTION_DENY;
}

