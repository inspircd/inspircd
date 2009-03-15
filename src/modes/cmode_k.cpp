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
#include "modes/cmode_k.h"

ModeChannelKey::ModeChannelKey(InspIRCd* Instance) : ModeHandler(Instance, 'k', 1, 1, false, MODETYPE_CHANNEL, false)
{
}

ModePair ModeChannelKey::ModeSet(User*, User*, Channel* channel, const std::string &parameter)
{
    if (channel->modes[CM_KEY])
    {
		std::string ckey = channel->GetModeParameter('k');
		return std::make_pair(true, ckey);
    }
    else
    {
		return std::make_pair(false, parameter);
    }
}

void ModeChannelKey::RemoveMode(Channel* channel, irc::modestacker* stack)
{
	/** +k needs a parameter when being removed,
	 * so we have a special-case RemoveMode here for it
	 */

	if (channel->IsModeSet('k'))
	{
		if (stack)
		{
			stack->Push('k', channel->GetModeParameter('k'));
		}
		else
		{
			std::vector<std::string> parameters;
			parameters.push_back(channel->name);
			parameters.push_back("-k");
			parameters.push_back(channel->GetModeParameter('k'));
			ServerInstance->SendMode(parameters, ServerInstance->FakeClient);
		}
	}
}

void ModeChannelKey::RemoveMode(User*, irc::modestacker* stack)
{
}

bool ModeChannelKey::CheckTimeStamp(time_t, time_t, const std::string &their_param, const std::string &our_param, Channel*)
{
	/* When TS is equal, the alphabetically later channel key wins */
	return (their_param < our_param);
}

ModeAction ModeChannelKey::OnModeChange(User* source, User*, Channel* channel, std::string &parameter, bool adding, bool servermode)
{
	bool exists = channel->IsModeSet('k');
	if (IS_LOCAL(source))
	{
		if (exists == adding)
			return MODEACTION_DENY;
		if (exists && (parameter != channel->GetModeParameter('k')))
		{
			/* Key is currently set and the correct key wasnt given */
			return MODEACTION_DENY;
		}
	} else {
		if (exists && adding && parameter == channel->GetModeParameter('k'))
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

	/* must first unset if we are changing the key, otherwise it will be ignored */
	if (exists && adding)
		channel->SetMode('k', false);

	/* must run setmode always, to process the change */
	channel->SetMode('k', adding);

	if (adding)
	{
		std::string ckey;
		ckey.assign(parameter, 0, 32);
		parameter = ckey;
		/* running this does not run setmode, despite the third parameter */
		channel->SetModeParam('k', parameter.c_str(), true);
	}
	return MODEACTION_ALLOW;
}
