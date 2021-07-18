/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2017-2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Shawn Smith <ShawnSmith0828@gmail.com>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006, 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006, 2008 Craig Edwards <brain@inspircd.org>
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
#include "modules/account.h"
#include "modules/callerid.h"
#include "modules/ctctags.h"
#include "modules/extban.h"
#include "modules/exemption.h"
#include "modules/whois.h"

enum
{
	// From ircd-hybrid?
	ERR_NEEDREGGEDNICK = 477,

	// From IRCv3 sasl-3.1.
	RPL_LOGGEDIN = 900,
	RPL_LOGGEDOUT = 901
};

/** Channel mode +r - mark a channel as identified
 */
class RegisteredChannel final
	: public SimpleChannelMode
{
 public:
	RegisteredChannel(Module* Creator)
		: SimpleChannelMode(Creator, "c_registered", 'r')
	{
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, Modes::Change& change) override
	{
		if (IS_LOCAL(source))
		{
			source->WriteNumeric(ERR_NOPRIVILEGES, "Only a server may modify the +r channel mode");
			return MODEACTION_DENY;
		}

		return SimpleChannelMode::OnModeChange(source, dest, channel, change);
	}
};

/** User mode +r - mark a user as identified
 */
class RegisteredUser final
	: public SimpleUserMode
{

 public:
	RegisteredUser(Module* Creator)
		: SimpleUserMode(Creator, "u_registered", 'r')
	{
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, Modes::Change& change) override
	{
		if (IS_LOCAL(source))
		{
			source->WriteNumeric(ERR_NOPRIVILEGES, "Only a server may modify the +r channel mode");
			return MODEACTION_DENY;
		}

		return SimpleUserMode::OnModeChange(source, dest, channel, change);
	}
};

class AccountExtItemImpl : public AccountExtItem
{
	Events::ModuleEventProvider eventprov;

 public:
	AccountExtItemImpl(Module* mod)
		: AccountExtItem(mod, "accountname", ExtensionItem::EXT_USER, true)
		, eventprov(mod, "event/account")
	{
	}

	void FromInternal(Extensible* container, const std::string& value) noexcept override
	{
		StringExtItem::FromInternal(container, value);
	}

	void FromNetwork(Extensible* container, const std::string& value) noexcept override
	{
		StringExtItem::FromNetwork(container, value);

		User* user = static_cast<User*>(container);
		if (IS_LOCAL(user))
		{
			if (value.empty())
			{
				// Logged out.
				user->WriteNumeric(RPL_LOGGEDOUT, user->GetFullHost(), "You are now logged out");
			}
			else
			{
				// Logged in.
				user->WriteNumeric(RPL_LOGGEDIN, user->GetFullHost(), value, InspIRCd::Format("You are now logged in as %s", value.c_str()));
			}
		}

		eventprov.Call(&AccountEventListener::OnAccountChange, user, value);
	}
};

class AccountExtBan
	: public ExtBan::MatchingBase
{
private:
	AccountExtItemImpl& accountext;

 public:
	AccountExtBan(Module* Creator, AccountExtItemImpl& AccountExt)
		: ExtBan::MatchingBase(Creator, "account", 'R')
		, accountext(AccountExt)
	{
	}

	bool IsMatch(User* user, Channel* channel, const std::string& text) override
	{
		const std::string* account = accountext.Get(user);
		return account && InspIRCd::Match(*account, text);
	}
};

class UnauthedExtBan
	: public ExtBan::MatchingBase
{
private:
	AccountExtItemImpl& accountext;

 public:
	UnauthedExtBan(Module* Creator, AccountExtItemImpl& AccountExt)
		: ExtBan::MatchingBase(Creator, "unauthed", 'U')
		, accountext(AccountExt)
	{
	}

	bool IsMatch(User* user, Channel* channel, const std::string& text) override
	{
		const std::string* account = accountext.Get(user);
		return !account && channel->CheckBan(user, text);
	}
};

class ModuleServicesAccount
	: public Module
	, public CTCTags::EventListener
	, public Whois::EventListener
{
 private:
	CallerID::API calleridapi;
	CheckExemption::EventProvider exemptionprov;
	SimpleChannelMode reginvitemode;
	SimpleChannelMode regmoderatedmode;
	SimpleUserMode regdeafmode;
	RegisteredChannel chanregmode;
	RegisteredUser userregmode;
	AccountExtItemImpl accountname;
	AccountExtBan accountextban;
	UnauthedExtBan unauthedextban;

 public:
	ModuleServicesAccount()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds various channel and user modes relating to services accounts.")
		, CTCTags::EventListener(this)
		, Whois::EventListener(this)
		, calleridapi(this)
		, exemptionprov(this)
		, reginvitemode(this, "reginvite", 'R')
		, regmoderatedmode(this, "regmoderated", 'M')
		, regdeafmode(this, "regdeaf", 'R')
		, chanregmode(this)
		, userregmode(this)
		, accountname(this)
		, accountextban(this, accountname)
		, unauthedextban(this, accountname)
	{
	}

	/* <- :twisted.oscnet.org 330 w00t2 w00t2 w00t :is logged in as */
	void OnWhois(Whois::Context& whois) override
	{
		std::string* account = accountname.Get(whois.GetTarget());

		if (account)
		{
			whois.SendLine(RPL_WHOISACCOUNT, *account, "is logged in as");
		}

		if (whois.GetTarget()->IsModeSet(userregmode))
		{
			/* user is registered */
			whois.SendLine(RPL_WHOISREGNICK, "is a registered nick");
		}
	}

	void OnUserPostNick(User* user, const std::string &oldnick) override
	{
		/* On nickchange, if they have +r, remove it */
		if ((user->IsModeSet(userregmode)) && (ServerInstance->Users.FindNick(oldnick) != user))
			userregmode.RemoveMode(user);
	}

	ModResult HandleMessage(User* user, const MessageTarget& target)
	{
		if (!IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		std::string *account = accountname.Get(user);
		bool is_registered = account && !account->empty();

		switch (target.type)
		{
			case MessageTarget::TYPE_CHANNEL:
			{
				Channel* targetchan = target.Get<Channel>();

				if (!targetchan->IsModeSet(regmoderatedmode) || is_registered)
					return MOD_RES_PASSTHRU;

				if (CheckExemption::Call(exemptionprov, user, targetchan, "regmoderated") == MOD_RES_ALLOW)
					return MOD_RES_PASSTHRU;

				// User is messaging a +M channel and is not registered or exempt.
				user->WriteNumeric(ERR_NEEDREGGEDNICK, targetchan->name, "You need to be identified to a registered account to message this channel");
				return MOD_RES_DENY;
			}
			case MessageTarget::TYPE_USER:
			{
				User* targetuser = target.Get<User>();
				if (!targetuser->IsModeSet(regdeafmode)  || is_registered)
					return MOD_RES_PASSTHRU;

				if (calleridapi && calleridapi->IsOnAcceptList(user, targetuser))
					return MOD_RES_PASSTHRU;

				// User is messaging a +R user and is not registered or on an accept list.
				user->WriteNumeric(ERR_NEEDREGGEDNICK, targetuser->nick, "You need to be identified to a registered account to message this user");
				return MOD_RES_DENY;
			}
			case MessageTarget::TYPE_SERVER:
				break;
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreMessage(User* user, const MessageTarget& target, MessageDetails& details) override
	{
		return HandleMessage(user, target);
	}

	ModResult OnUserPreTagMessage(User* user, const MessageTarget& target, CTCTags::TagMessageDetails& details) override
	{
		return HandleMessage(user, target);
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven, bool override) override
	{
		if (override)
			return MOD_RES_PASSTHRU;

		std::string *account = accountname.Get(user);
		bool is_registered = account && !account->empty();

		if (chan)
		{
			if (chan->IsModeSet(reginvitemode))
			{
				if (!is_registered)
				{
					// joining a +R channel and not identified
					user->WriteNumeric(ERR_NEEDREGGEDNICK, chan->name, "You need to be identified to a registered account to join this channel");
					return MOD_RES_DENY;
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnSetConnectClass(LocalUser* user, std::shared_ptr<ConnectClass> myclass) override
	{
		if (myclass->config->getBool("requireaccount") && !accountname.Get(user))
		{
			ServerInstance->Logs.Log("CONNECTCLASS", LOG_DEBUG, "The %s connect class is not suitable as it requires the user to be logged into an account",
				myclass->GetName().c_str());
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleServicesAccount)
