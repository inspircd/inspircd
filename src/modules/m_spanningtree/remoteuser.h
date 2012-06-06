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


#ifndef M_SPANNINGTREE_REMOTEUSER_H
#define M_SPANNINGTREE_REMOTEUSER_H
class RemoteUser : public User
{
 public:
	TreeServer* srv;
	RemoteUser(const std::string& uid, TreeServer* Srv) :
		User(uid, Srv->GetName(), USERTYPE_REMOTE), srv(Srv)
	{
	}
	virtual void SendText(const std::string& line);
	virtual void DoWhois(User* src);
};

#endif
