#include "mode.h"

class ModeUserInvisible : public ModeHandler
{
 public:
	ModeUserInvisible();
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
};
