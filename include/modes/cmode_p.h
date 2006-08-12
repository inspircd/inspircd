#include "mode.h"

class InspIRCd;

/** Channel mode +p
 */
class ModeChannelPrivate : public ModeHandler
{
 public:
	ModeChannelPrivate(InspIRCd* Instance);
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
};
