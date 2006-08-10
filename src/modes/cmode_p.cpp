#include "inspircd.h"
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modes/cmode_p.h"

ModeChannelPrivate::ModeChannelPrivate(InspIRCd* Instance) : ModeHandler(Instance, 'p', 0, 0, false, MODETYPE_CHANNEL, false)
{
}

ModeAction ModeChannelPrivate::OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
{
	if (channel->modes[CM_PRIVATE] != adding)
	{
		channel->modes[CM_PRIVATE] = adding;
		return MODEACTION_ALLOW;
	}
	else
	{
		return MODEACTION_DENY;
	}
}
