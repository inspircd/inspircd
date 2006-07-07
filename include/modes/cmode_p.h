#include "mode.h"

class ModeChannelPrivate : public ModeHandler
{
 public:
	ModeChannelPrivate();
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
};
