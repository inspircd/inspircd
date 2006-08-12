#include "mode.h"

class InspIRCd;

/** User mode +i
 */
class ModeUserInvisible : public ModeHandler
{
 public:
	ModeUserInvisible(InspIRCd* Instance);
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
};
