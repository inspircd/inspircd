#include "mode.h"

class ModeChannelKey : public ModeHandler
{
 public:
	ModeChannelKey();
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
	std::pair<bool,std::string> ModeSet(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter);
};
