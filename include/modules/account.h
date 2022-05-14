/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022 Sadie Powell <sadie@witchery.services>
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


#pragma once

#include "event.h"

namespace Account
{
	class API;
	class APIBase;
	class EventListener;
}

/** Defines the interface for the account API. */
class Account::APIBase
	: public DataProvider
{
public:
	APIBase(Module* parent)
		: DataProvider(parent, "accountapi")
	{
	}

	/** Retrieves the account identifier of the specified user.
	 * @param user The user to retrieve the account identifier of.
	 * @return If the user is logged in to an account then the account identifier; otherwise, nullptr.
	 */
	virtual std::string* GetAccountId(const User* user) const = 0;

	/** Retrieves the account name of the specified user.
	 * @param user The user to retrieve the account name of.
	 * @return If the user is logged in to an account then the account name; otherwise, nullptr.
	 */
	virtual std::string* GetAccountName(const User* user) const = 0;
};

/** Allows modules to access information regarding user accounts. */
class Account::API final
	: public dynamic_reference<Account::APIBase>
{
public:
	API(Module* parent)
		: dynamic_reference<Account::APIBase>(parent, "accountapi")
	{
	}

};

/** Provides handlers for events relating to accounts. */
class Account::EventListener
	: public Events::ModuleEventListener
{
public:
	EventListener(Module* mod)
		: ModuleEventListener(mod, "event/account")
	{
	}

	/** Called whenever a user logs in or out of an account.
	 * @param user The user who logged in or out.
	 * @param account The name of the account if logging in or empty if logging out.
	 */
	virtual void OnAccountChange(User* user, const std::string& account) = 0;
};
