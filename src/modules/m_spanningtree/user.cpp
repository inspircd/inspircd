#include "inspircd.h"
#include "treeserver.h"
#include "remoteuser.h"

void RemoteUser::SendText(const std::string& line)
{
	char buf[MAXBUF+30];
	snprintf(buf, MAXBUF+30, ":%s PUSH %s :%s",
		ServerInstance->Config->GetSID().c_str(), uuid.c_str(), line.c_str());
	TreeSocket* sock = srv->GetSocket();
	sock->WriteLine(buf);
}

void RemoteUser::DoWhois(User* src)
{
	char buf[MAXBUF];
	snprintf(buf, MAXBUF, ":%s IDLE %s", src->uuid.c_str(), this->uuid.c_str());
	TreeSocket* sock = srv->GetSocket();
	sock->WriteLine(buf);
}
