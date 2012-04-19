/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
