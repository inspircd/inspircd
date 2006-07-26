#include "mode.h"
#include "channels.h"

class ModeChannelOp : public ModeHandler
{
 private:
 public:
	ModeChannelOp();
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
	std::string AddOp(userrec *user,const char *dest,chanrec *chan,int status);
	std::string DelOp(userrec *user,const char *dest,chanrec *chan,int status);
	std::pair<bool,std::string> ModeSet(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter);
};

