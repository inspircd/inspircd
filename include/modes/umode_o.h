#include "mode.h"

class InspIRCd;

class ModeUserOperator : public ModeHandler
{
 public:
	ModeUserOperator(InspIRCd* Instance);
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
};
