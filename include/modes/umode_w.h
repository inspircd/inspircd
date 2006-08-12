#include "mode.h"

class InspIRCd;

/** User mode +w
 */
class ModeUserWallops : public ModeHandler
{
 public:
	ModeUserWallops(InspIRCd* Instance);
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
};
