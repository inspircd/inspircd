/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modes/umode_s.h"

ModeUserServerNoticeMask::ModeUserServerNoticeMask() : ModeHandler(NULL, "snomask", 's', PARAM_SETONLY, MODETYPE_USER)
{
	oper = true;
}

ModeAction ModeUserServerNoticeMask::OnModeChange(User* source, User* dest, Channel*, std::string &parameter, bool adding)
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

