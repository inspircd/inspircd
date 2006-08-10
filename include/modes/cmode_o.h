#include "mode.h"
#include "channels.h"

class InspIRCd;

class ModeChannelOp : public ModeHandler
{
 private:
 public:
	ModeChannelOp(InspIRCd* Instance);
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
	std::string AddOp(userrec *user,const char *dest,chanrec *chan,int status);
	std::string DelOp(userrec *user,const char *dest,chanrec *chan,int status);
	ModePair ModeSet(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter);
};

