/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2006-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2006, 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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
#include "protocol.h"
#include "account.h"

/* $ModDesc: Povides support for accounts. */

struct AccountItem
{
	irc::string account;
	std::string tag;
	AccountItem(const irc::string& a) : account(a) {}
	AccountItem(const irc::string& a, const std::string& t) : account(a), tag(t) {}
	std::string metastr()
	{
		if (!this)
			return "";
		if (tag.empty())
			return account;
		return std::string(account) + " " + tag;
	}
};

class ServicesExtItem : public SimpleExtItem<AccountItem>
{
 public:
	ServicesExtItem(Module* owner) : SimpleExtItem<AccountItem>(EXTENSIBLE_USER, "accountname", owner) {}

	std::string serialize(SerializeFormat format, const Extensible* container, void* itemv) const
	{
		return static_cast<AccountItem*>(itemv)->metastr();
	}

	void unserialize(SerializeFormat format, Extensible* container, const std::string& value)
	{
		std::string::size_type spc = value.find(' ');
		if (value.empty())
			unset(container);
		else if (spc == std::string::npos)
			set(container, new AccountItem(value));
		else
			set(container, new AccountItem(value.substr(0, spc), value.substr(spc + 1)));
	}
};


class ServicesAccountProvider : public AccountProvider
{
 public:
	ServicesExtItem ext;
	ServicesAccountProvider(Module* mod)
		: AccountProvider(mod, "account/services_account"), ext(mod)
	{
	}

	bool IsRegistered(User* user)
	{
		return ext.get(user);
	}

	irc::string GetAccountName(User* user)
	{
		AccountItem* account = ext.get(user);
		if (account)
			return account->account;
		return "";
	}

	std::string GetAccountTag(User* user)
	{
		AccountItem* account = ext.get(user);
		if (account)
			return account->tag;
		return "";
	}

	void DoLogin(User* user, const irc::string& acct, const std::string& tag)
	{
		if (IS_LOCAL(user) && !acct.empty())
			user->WriteNumeric(900, "%s %s %s :You are now logged in as %s",
				user->nick.c_str(), user->GetFullHost().c_str(), acct.c_str(), acct.c_str());

		AccountItem* item = NULL;
		if (acct.empty())
		{
			ext.unset(user);
		}
		else
		{
			item = new AccountItem(acct, tag);
			ext.set(user, item);
		}

		if (user->registered == REG_ALL)
			ServerInstance->PI->SendMetaData(user, "accountname", item->metastr());

		AccountEvent(creator, user, acct).Send();
	}
};

class ModuleServicesAccount : public Module
{
	ServicesAccountProvider account;
 public:
	ModuleServicesAccount() : account(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(account);
		ServerInstance->Modules->AddService(account.ext);
		Implementation eventlist[] = {
			I_OnWhois, I_OnDecodeMetaData, I_OnSetConnectClass
		};
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	/* <- :twisted.oscnet.org 330 w00t2 w00t2 w00t :is logged in as */
	void OnWhois(User* source, User* dest)
	{
		irc::string acctname = account.GetAccountName(dest);

		if (!acctname.empty())
		{
			ServerInstance->SendWhoisLine(source, dest, 330, "%s %s %s :is logged in as",
				source->nick.c_str(), dest->nick.c_str(), acctname.c_str());
		}
	}

	// Whenever the linking module receives metadata from another server and doesnt know what
	// to do with it (of course, hence the 'meta') it calls this method, and it is up to each
	// module in turn to figure out if this metadata key belongs to them, and what they want
	// to do with it.
	// In our case we're only sending a single string around, so we just construct a std::string.
	// Some modules will probably get much more complex and format more detailed structs and classes
	// in a textual way for sending over the link.
	void OnDecodeMetaData(Extensible* target, const std::string &extname, const std::string &extdata)
	{
		User* dest = IS_USER(target);
		// check if its our metadata key, and its associated with a user
		if (dest && extname == "accountname")
		{
			irc::string acct = account.GetAccountName(dest);
			if (IS_LOCAL(dest) && !acct.empty())
				dest->WriteNumeric(900, "%s %s %s :You are now logged in as %s",
					dest->nick.c_str(), dest->GetFullHost().c_str(), acct.c_str(), acct.c_str());

			AccountEvent(this, dest, acct).Send();
		}
	}

	ModResult OnSetConnectClass(LocalUser* user, ConnectClass* myclass)
	{
		if (myclass->config->getBool("requireaccount") && !account.IsRegistered(user))
			return MOD_RES_DENY;
		return MOD_RES_PASSTHRU;
	}

	Version GetVersion()
	{
		return Version("Povides support for accounts.",VF_OPTCOMMON|VF_VENDOR);
	}
};

MODULE_INIT(ModuleServicesAccount)
