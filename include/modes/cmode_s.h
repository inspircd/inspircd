#include "mode.h"

class InspIRCd;

/** Channel mode +s
 */
class ModeChannelSecret : public ModeHandler
{
 public:
	ModeChannelSecret(InspIRCd* Instance);
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
};
