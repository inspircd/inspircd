/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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


#ifndef MEMBERSHIP_H
#define MEMBERSHIP_H

class CoreExport Membership : public Extensible
{
 public:
	User* const user;
	Channel* const chan;
	// mode list, sorted by prefix rank, higest first
	std::string modes;
	Membership(User* u, Channel* c) : user(u), chan(c) {}
	inline bool hasMode(char m) const
	{
		return modes.find(m) != std::string::npos;
	}
	unsigned int getRank();
};

class CoreExport InviteBase
{
 protected:
	InviteList invites;

 public:
	void ClearInvites();

	friend class Invitation;
};

class Invitation : public classbase
{
	Invitation(Channel* c, LocalUser* u, time_t timeout) : user(u), chan(c), expiry(timeout) {}

 public:
	LocalUser* const user;
	Channel* const chan;
	time_t expiry;

	~Invitation();
	static void Create(Channel* c, LocalUser* u, time_t timeout);
	static Invitation* Find(Channel* c, LocalUser* u, bool check_expired = true);
};

#endif
