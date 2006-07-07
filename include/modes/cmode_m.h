#include "mode.h"

class ModeChannelModerated : public ModeHandler
{
 public:
	ModeChannelModerated();
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
};
