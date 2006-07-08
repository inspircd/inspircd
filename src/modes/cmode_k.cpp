#include "inspircd.h"
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modes/cmode_k.h"

ModeChannelKey::ModeChannelKey() : ModeHandler('k', 1, 1, false, MODETYPE_CHANNEL, false)
{
}

ModeAction ModeChannelKey::OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
{
	if (channel->modes[CM_KEY] != adding)
	{
		if ((channel->modes[CM_KEY]) && (strcasecmp(parameter.c_str(),channel->key)))
		{
			/* Key is currently set and the correct key wasnt given */
			return MODEACTION_DENY;
		}
		else if (!channel->modes[CM_KEY])
		{
			/* Key isnt currently set */
			strlcpy(channel->key,parameter.c_str(),32);
			channel->modes[CM_KEY] = adding;
			return MODEACTION_ALLOW;
		}
		else if ((channel->modes[CM_KEY]) && (!strcasecmp(parameter.c_str(),channel->key)))
		{
			/* Key is currently set, and correct key was given */
			*channel->key = 0;
			channel->modes[CM_KEY] = adding;
			return MODEACTION_ALLOW;
		}
		return MODEACTION_DENY;
	}
	else
	{
		return MODEACTION_DENY;
	}
}
