#include "mode.h"

class InspIRCd;

class ModeChannelTopicOps : public ModeHandler
{
 public:
	ModeChannelTopicOps(InspIRCd* Instance);
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
};
