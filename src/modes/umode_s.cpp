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
#include "modes/umode_s.h"

ModeUserServerNoticeMask::ModeUserServerNoticeMask(InspIRCd* Instance) : ModeHandler(Instance, NULL, 's', 1, 0, false, MODETYPE_USER, true)
{
}

ModeAction ModeUserServerNoticeMask::OnModeChange(User* source, User* dest, Channel*, std::string &parameter, bool adding, bool servermode)
{
	/* Set the array fields */
	if (adding)
	{
		/* Fix for bug #310 reported by Smartys */
		if (!dest->modes[UM_SNOMASK])
			dest->snomasks.reset();

		dest->modes[UM_SNOMASK] = true;
		parameter = dest->ProcessNoticeMasks(parameter.c_str());
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

std::string ModeUserServerNoticeMask::GetUserParameter(User* user)
{
	std::string masks = user->FormatNoticeMasks();
	if (masks.length())
		masks = "+" + masks;
	return masks;
}

void ModeUserServerNoticeMask::OnParameterMissing(User* user, User* dest, Channel* channel)
{
	user->WriteServ("NOTICE %s :*** The user mode +s requires a parameter (server notice mask). Please provide a parameter, e.g. '+s +*'.",
			user->nick.c_str());
}

