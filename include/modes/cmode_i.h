#include "mode.h"

class ModeChannelInviteOnly : public ModeHandler
{
 public:
	ModeChannelInviteOnly();
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
};
