#include "mode.h"

class ModeUserServerNotice : public ModeHandler
{
 public:
	ModeUserServerNotice();
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
};
