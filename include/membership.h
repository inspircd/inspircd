/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __MEMBERSHIP_H__
#define __MEMBERSHIP_H__

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
