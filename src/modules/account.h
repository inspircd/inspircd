/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <map>
#include <string>

class AccountEvent : public Event
{
 public:
	User* const user;
	const std::string account;
	AccountEvent(Module* me, User* u, const std::string& name)
		: Event(me, "account_login"), user(u), account(name)
	{
	}
};

typedef StringExtItem AccountExtItem;

inline AccountExtItem* GetAccountExtItem()
{
	return static_cast<AccountExtItem*>(ServerInstance->Extensions.GetItem("accountname"));
}

#endif
