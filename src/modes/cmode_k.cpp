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
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "builtin-modes.h"

ModeChannelKey::ModeChannelKey() : ModeHandler(NULL, "key", 'k', PARAM_ALWAYS, MODETYPE_CHANNEL)
{
}

ModeAction ModeChannelKey::OnModeChange(User* source, User*, Channel* channel, std::string &parameter, bool adding)
{
	bool exists = channel->IsModeSet(this);
	if (IS_LOCAL(source))
	{
		if (exists == adding)
			return MODEACTION_DENY;
		if (exists && (parameter != channel->GetModeParameter(this)))
		{
			/* Key is currently set and the correct key wasnt given */
			return MODEACTION_DENY;
		}
	} else {
		if (exists && adding && parameter == channel->GetModeParameter(this))
		{
			/* no-op, don't show */
			return MODEACTION_DENY;
		}
	}

	/* invalid keys */
	if (!parameter.length())
		return MODEACTION_DENY;

	if (parameter.rfind(' ') != std::string::npos)
		return MODEACTION_DENY;

	if (adding)
	{
		std::string ckey;
		ckey.assign(parameter, 0, 32);
		parameter = ckey;
		channel->SetModeParam(this, parameter);
	}
	else
	{
		channel->SetModeParam(this, "");
	}
	return MODEACTION_ALLOW;
}
