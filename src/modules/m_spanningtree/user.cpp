#include "inspircd.h"
#include "treeserver.h"
#include "remoteuser.h"

void RemoteUser::SendText(const std::string& line)
{
	char buf[MAXBUF+30];
	snprintf(buf, MAXBUF+30, ":%s PUSH %s :%s",
		ServerInstance->Config->GetSID().c_str(), uuid.c_str(), line.c_str());
	TreeSocket* sock = srv->GetRoute()->GetSocket();
	sock->WriteLine(buf);
}
