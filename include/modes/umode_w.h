#include "mode.h"

class ModeUserWallops : public ModeHandler
{
 public:
	ModeUserWallops();
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
};
