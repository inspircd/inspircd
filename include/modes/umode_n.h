#include "mode.h"

class InspIRCd;

class ModeUserServerNoticeMask : public ModeHandler
{
 public:
	ModeUserServerNoticeMask(InspIRCd* Instance);
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
};
