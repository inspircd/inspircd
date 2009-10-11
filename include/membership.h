/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __MEMBERSHIP_H__
#define __MEMBERSHIP_H__

class User;
class Channel;

struct CoreExport Membership : public Extensible
{
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

CoreExport typedef std::map<User*, Membership*> UserMembList;
CoreExport typedef UserMembList::iterator UserMembIter;
CoreExport typedef UserMembList::const_iterator UserMembCIter;

CoreExport typedef std::set<User*> CUList;

#endif
