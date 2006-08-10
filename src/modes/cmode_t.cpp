#include "inspircd.h"
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modes/cmode_t.h"

ModeChannelTopicOps::ModeChannelTopicOps(InspIRCd* Instance) : ModeHandler(Instance, 't', 0, 0, false, MODETYPE_CHANNEL, false)
{
}

ModeAction ModeChannelTopicOps::OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
{
	if (channel->modes[CM_TOPICLOCK] != adding)
	{
		channel->modes[CM_TOPICLOCK] = adding;
		return MODEACTION_ALLOW;
	}
	else
	{
		return MODEACTION_DENY;
	}
}

