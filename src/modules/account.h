/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
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
