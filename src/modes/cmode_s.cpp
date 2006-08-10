#include "inspircd.h"
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modes/cmode_s.h"

ModeChannelSecret::ModeChannelSecret(InspIRCd* Instance) : ModeHandler(Instance, 's', 0, 0, false, MODETYPE_CHANNEL, false)
{
}

ModeAction ModeChannelSecret::OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
{
	if (channel->modes[CM_SECRET] != adding)
	{
		channel->modes[CM_SECRET] = adding;
		return MODEACTION_ALLOW;
	}
	else
	{
		return MODEACTION_DENY;
	}
}
