#include "mode.h"
#include "channels.h"

class ModeChannelBan : public ModeHandler
{
 private:
	BanItem b;
 public:
	ModeChannelBan();
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding);
	std::string& AddBan(userrec *user,std::string& dest,chanrec *chan,int status);
	std::string& DelBan(userrec *user,std::string& dest,chanrec *chan,int status);
	void DisplayList(userrec* user, chanrec* channel);
};

