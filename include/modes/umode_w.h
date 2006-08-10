#include "mode.h"

class InspIRCd;

class ModeUserWallops : public ModeHandler
{
 public:
	ModeUserWallops(InspIRCd* Instance);
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
};
