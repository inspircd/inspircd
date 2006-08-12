#include "mode.h"

class InspIRCd;

/** User mode +s
 */
class ModeUserServerNotice : public ModeHandler
{
 public:
	ModeUserServerNotice(InspIRCd* Instance);
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
};
