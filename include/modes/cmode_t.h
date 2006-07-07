#include "mode.h"

class ModeChannelTopicOps : public ModeHandler
{
 public:
	ModeChannelTopicOps();
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
};
