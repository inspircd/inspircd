#include "mode.h"

class ModeUserServerNoticeMask : public ModeHandler
{
 public:
	ModeUserServerNoticeMask();
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
};
