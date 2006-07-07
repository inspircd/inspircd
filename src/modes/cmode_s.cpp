#include "inspircd.h"
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modes/cmode_s.h"

ModeChannelSecret::ModeChannelSecret() : ModeHandler('s', 0, 0, 0, MODETYPE_CHANNEL, false)
{
}

ModeAction ModeChannelSecret::OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
{
	channel->modes[CM_SECRET] = adding;
	return MODEACTION_ALLOW;
}
