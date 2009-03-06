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

	if (channel->IsModeSet(this->GetModeChar()))
	{
		if (stack)
		{
			stack->Push(this->GetModeChar(), channel->GetModeParameter('k'));
		}
		else
		{
			std::vector<std::string> parameters; parameters.push_back(channel->name); parameters.push_back("-k"); parameters.push_back(channel->GetModeParameter('k'));
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
	if ((channel->IsModeSet('k') != adding) || (!IS_LOCAL(source)))
	{
		if (((channel->IsModeSet('k')) && (parameter != channel->GetModeParameter('k'))) && (IS_LOCAL(source)))
		{
			/* Key is currently set and the correct key wasnt given */
			return MODEACTION_DENY;
		}
		else if (channel->IsModeSet('k') && parameter == channel->GetModeParameter('k'))
		{
			/* Key is currently set,  setting to the same as it already is.. drop it */
			return MODEACTION_DENY;
		}
		else if ((!channel->IsModeSet('k')) || ((adding) && (!IS_LOCAL(source))))
		{
			/* Key isnt currently set */
			if ((parameter.length()) && (parameter.rfind(' ') == std::string::npos))
			{
				std::string ckey;
				ckey.assign(parameter, 0, 32);
				channel->SetModeParam('k', ckey.c_str(), adding);
				channel->SetMode('k', adding);
				parameter = ckey;
				return MODEACTION_ALLOW;
			}
			else
				return MODEACTION_DENY;
		}
		else if (((channel->IsModeSet('k')) && (parameter == channel->GetModeParameter('k'))) || ((!adding) && (!IS_LOCAL(source))))
		{
			/* Key is currently set, and correct key was given */
			channel->SetMode('k', adding);
			return MODEACTION_ALLOW;
		}
		return MODEACTION_DENY;
	}
	else
	{
		return MODEACTION_DENY;
	}
}
