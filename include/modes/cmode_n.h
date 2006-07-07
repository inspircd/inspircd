#include "mode.h"

class ModeChannelNoExternal : public ModeHandler
{
 public:
	ModeChannelNoExternal();
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
};
