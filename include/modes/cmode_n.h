#include "mode.h"

class InspIRCd;

/** Channel mode +n
 */
class ModeChannelNoExternal : public ModeHandler
{
 public:
	ModeChannelNoExternal(InspIRCd* Instance);
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
};
