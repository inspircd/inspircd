#include "inspircd.h"
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modes/cmode_m.h"

ModeChannelModerated::ModeChannelModerated(InspIRCd* Instance) : ModeHandler(Instance, 'm', 0, 0, false, MODETYPE_CHANNEL, false)
{
}

ModeAction ModeChannelModerated::OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
{
	if (channel->modes[CM_MODERATED] != adding)
	{
		channel->modes[CM_MODERATED] = adding;
		return MODEACTION_ALLOW;
	}
	else
	{
		return MODEACTION_DENY;
	}
}

