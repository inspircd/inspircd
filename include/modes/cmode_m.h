#include "mode.h"

class InspIRCd;

class ModeChannelModerated : public ModeHandler
{
 public:
	ModeChannelModerated(InspIRCd* Instance);
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
};
