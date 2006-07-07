#include "inspircd.h"
#include "mode.h"
#include "channels.h"
#include "users.h"

class ModeChannelSecret : public ModeHandler
{
 public:
	ModeChannelSecret() : ModeHandler('s', 0, 0, 0, MODETYPE_CHANNEL, false)
	{
	}

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		channel->modes[CM_SECRET] = adding;
		return MODEACTION_ALLOW;
	}
};

