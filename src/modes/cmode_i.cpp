#include "inspircd.h"
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modes/cmode_i.h"

ModeChannelInviteOnly::ModeChannelInviteOnly() : ModeHandler('i', 0, 0, false, MODETYPE_CHANNEL, false)
{
}

ModeAction ModeChannelInviteOnly::OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
{
	if (channel->modes[CM_INVITEONLY] != adding)
	{
		channel->modes[CM_INVITEONLY] = adding;
		return MODEACTION_ALLOW;
	}
	else
	{
		return MODEACTION_DENY;
	}
}
