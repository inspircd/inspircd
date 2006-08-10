#include "mode.h"
#include "channels.h"

class InspIRCd;

class ModeChannelVoice : public ModeHandler
{
 private:
 public:
	ModeChannelVoice(InspIRCd* Instance);
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
	std::string AddVoice(userrec *user,const char *dest,chanrec *chan,int status);
	std::string DelVoice(userrec *user,const char *dest,chanrec *chan,int status);
	ModePair ModeSet(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter);
};

