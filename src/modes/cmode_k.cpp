#include "inspircd.h"
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modes/cmode_k.h"

ModeChannelKey::ModeChannelKey(InspIRCd* Instance) : ModeHandler(Instance, 'k', 1, 1, false, MODETYPE_CHANNEL, false)
{
}

ModePair ModeChannelKey::ModeSet(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter)
{       
        if (channel->modes[CM_KEY])
        {
                return std::make_pair(true, channel->key);
        }
        else
        {
                return std::make_pair(false, parameter);
        }
}

void ModeChannelKey::RemoveMode(chanrec* channel)
{
	char moderemove[MAXBUF];
	const char* parameters[] = { channel->name, moderemove, channel->key };

	if (channel->IsModeSet(this->GetModeChar()))
	{
		userrec* n = new userrec(ServerInstance);

		sprintf(moderemove,"-%c",this->GetModeChar());
		n->SetFd(FD_MAGIC_NUMBER);

		ServerInstance->SendMode(parameters, 3, n);

		delete n;
	}
}

void ModeChannelKey::RemoveMode(userrec* user)
{
}

bool ModeChannelKey::CheckTimeStamp(time_t theirs, time_t ours, const std::string &their_param, const std::string &our_param, chanrec* channel)
{
	/* When TS is equal, the alphabetically later channel key wins */
	return (their_param < our_param);
}

ModeAction ModeChannelKey::OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
{
	if ((channel->modes[CM_KEY] != adding) || (!IS_LOCAL(source)))
	{
		if (((channel->modes[CM_KEY]) && (strcasecmp(parameter.c_str(),channel->key))) && (IS_LOCAL(source)))
		{
			/* Key is currently set and the correct key wasnt given */
			ServerInstance->Log(DEBUG,"Key Cond 2");
			return MODEACTION_DENY;
		}
		else if ((!channel->modes[CM_KEY]) || ((adding) && (!IS_LOCAL(source))))
		{
			/* Key isnt currently set */
			if ((parameter.length()) && (parameter.rfind(' ') == std::string::npos))
			{
				strlcpy(channel->key,parameter.c_str(),32);
				channel->modes[CM_KEY] = adding;
				return MODEACTION_ALLOW;
			}
			else
				return MODEACTION_DENY;
		}
		else if (((channel->modes[CM_KEY]) && (!strcasecmp(parameter.c_str(),channel->key))) || ((!adding) && (!IS_LOCAL(source))))
		{
			/* Key is currently set, and correct key was given */
			*channel->key = 0;
			channel->modes[CM_KEY] = adding;
			return MODEACTION_ALLOW;
		}
		ServerInstance->Log(DEBUG,"Key Cond three");
		return MODEACTION_DENY;
	}
	else
	{
		ServerInstance->Log(DEBUG,"Key Condition one");
		return MODEACTION_DENY;
	}
}
