#include "mode.h"

class ModeChannelSecret : public ModeHandler
{
 public:
	ModeChannelSecret();
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
};
