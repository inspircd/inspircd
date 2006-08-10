#include "mode.h"

class InspIRCd;

class ModeChannelNoExternal : public ModeHandler
{
 public:
	ModeChannelNoExternal(InspIRCd* Instance);
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
};
