#include "mode.h"

class InspIRCd;

/** Channel mode +i
 */
class ModeChannelInviteOnly : public ModeHandler
{
 public:
	ModeChannelInviteOnly(InspIRCd* Instance);
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
};
