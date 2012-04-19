/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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
	Membership* u_prev;
	Membership* u_next;
 public:
	User* const user;
	Channel* const chan;
	// mode list, sorted by prefix rank, higest first
	std::string modes;
	Membership(User* u, Channel* c) : Extensible(EXTENSIBLE_MEMBERSHIP), u_prev(NULL), u_next(NULL), user(u), chan(c) {}
	inline bool hasMode(char m) const
	{
		return modes.find(m) != std::string::npos;
	}
	/** Rank in the channel for actions this user is taking */
	unsigned int GetAccessRank();
	/** Rank in the channel for actions performed on this user */
	unsigned int GetProtectRank();
	friend class UCListIter;
	friend class UserChanList;
};

class CoreExport UCListIter
{
	Membership* curr;
 public:
	UCListIter(Membership* i) : curr(i) {}
	inline void operator++() { if (curr) curr = curr->u_next; }
	inline void operator++(int) { if (curr) curr = curr->u_next; }
	inline bool operator==(const UCListIter& o) const { return curr == o.curr; }
	inline bool operator!=(const UCListIter& o) const { return curr != o.curr; }
	inline operator Membership*() const { return curr; }
	inline Membership* operator->() const { return curr; }
	inline Membership& operator*() const { return *curr; }
};

class CoreExport UserChanList : public interfacebase
{
	Membership* head;
	size_t siz;
 public:
	UserChanList() : head(NULL), siz(0) {}
	inline UCListIter begin() { return head; }
	inline UCListIter end() { return NULL; }
	inline void insert(Membership* m)
	{
		siz++;
		if (head)
		{
			m->u_next = head;
			head->u_prev = m;
		}
		head = m;
	}
	inline void erase(Membership* m)
	{
		siz--;
		if (head == m)
			head = m->u_next;
		if (m->u_next)
			m->u_next->u_prev = m->u_prev;
		if (m->u_prev)
			m->u_prev->u_next = m->u_next;
		m->u_prev = m->u_next = NULL;
	}
	inline size_t size() { return siz; }
};

#endif
