/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
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
#include "modes/umode_s.h"

ModeUserServerNoticeMask::ModeUserServerNoticeMask(InspIRCd* Instance) : ModeHandler(Instance, 's', 1, 0, false, MODETYPE_USER, true)
{
}

ModeAction ModeUserServerNoticeMask::OnModeChange(User* source, User* dest, Channel*, std::string &parameter, bool adding, bool servermode)
{
	/* Only opers can change other users modes */
	if ((source != dest) && (!IS_OPER(source)))
		return MODEACTION_DENY;

	/* Set the array fields */
	if (adding)
	{
		/* Fix for bug #310 reported by Smartys */
		if (!dest->modes[UM_SNOMASK])
			dest->snomasks.reset();

		parameter = dest->ProcessNoticeMasks(parameter.c_str());
		dest->modes[UM_SNOMASK] = true;
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

void ModeUserServerNoticeMask::OnParameterMissing(User* user, User* dest, Channel* channel)
{
	user->WriteServ("NOTICE %s :*** The user mode +s requires a parameter (server notice mask). Please provide a parameter, e.g. '+s +*'.",
			user->nick.c_str());
}

