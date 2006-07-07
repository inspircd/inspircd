#include "inspircd.h"
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modes/cmode_n.h"

ModeChannelNoExternal::ModeChannelNoExternal() : ModeHandler('n', 0, 0, false, MODETYPE_CHANNEL, false)
{
}

ModeAction ModeChannelNoExternal::OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
{
	if (channel->modes[CM_NOEXTERNAL] != adding)
	{
		channel->modes[CM_NOEXTERNAL] = adding;
		return MODEACTION_ALLOW;
	}
	else
	{
		return MODEACTION_DENY;
	}
}

