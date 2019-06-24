/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012 Shawn Smith <shawn@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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
#include "modules/account.h"
#include "modules/callerid.h"
#include "modules/ctctags.h"
#include "modules/exemption.h"
#include "modules/whois.h"

enum
{
	// From UnrealIRCd.
	RPL_WHOISREGNICK = 307,

	// From ircu.
	RPL_WHOISACCOUNT = 330,

	// From ircd-hybrid?
	ERR_NEEDREGGEDNICK = 477,

	// From IRCv3 sasl-3.1.
	RPL_LOGGEDIN = 900,
	RPL_LOGGEDOUT = 901
};

/** Channel mode +r - mark a channel as identified
 */
class Channel_r : public ModeHandler
{
 public:
	Channel_r(Module* Creator) : ModeHandler(Creator, "c_registered", 'r', PARAM_NONE, MODETYPE_CHANNEL) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string& parameter, bool adding) CXX11_OVERRIDE
	{
		// Only a U-lined server may add or remove the +r mode.
		if (!IS_LOCAL(source))
		{
			// Only change the mode if it's not redundant
			if ((adding != channel->IsModeSet(this)))
			{
				channel->SetMode(this, adding);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			source->WriteNumeric(ERR_NOPRIVILEGES, "Only a server may modify the +r channel mode");
		}
		return MODEACTION_DENY;
	}
};

/** User mode +r - mark a user as identified
 */
class User_r : public ModeHandler
{

 public:
	User_r(Module* Creator) : ModeHandler(Creator, "u_registered", 'r', PARAM_NONE, MODETYPE_USER) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string& parameter, bool adding) CXX11_OVERRIDE
	{
		if (!IS_LOCAL(source))
		{
			if ((adding != dest->IsModeSet(this)))
			{
				dest->SetMode(this, adding);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			source->WriteNumeric(ERR_NOPRIVILEGES, "Only a server may modify the +r user mode");
		}
		return MODEACTION_DENY;
	}
};

class AccountExtItemImpl : public AccountExtItem
{
	Events::ModuleEventProvider eventprov;

 public:
	AccountExtItemImpl(Module* mod)
		: AccountExtItem("accountname", ExtensionItem::EXT_USER, mod)
		, eventprov(mod, "event/account")
	{
	}

	void unserialize(SerializeFormat format, Extensible* container, const std::string& value) CXX11_OVERRIDE
	{
		User* user = static_cast<User*>(container);

		StringExtItem::unserialize(format, container, value);

		// If we are being reloaded then don't send the numeric or run the event
		if (format == FORMAT_INTERNAL)
			return;

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

		FOREACH_MOD_CUSTOM(eventprov, AccountEventListener, OnAccountChange, (user, value));
	}
};

class ModuleServicesAccount
	: public Module
	, public Whois::EventListener
	, public CTCTags::EventListener
{
 private:
	CallerID::API calleridapi;
	CheckExemption::EventProvider exemptionprov;
	SimpleChannelModeHandler m1;
	SimpleChannelModeHandler m2;
	SimpleUserModeHandler m3;
	Channel_r m4;
	User_r m5;
	AccountExtItemImpl accountname;
	bool checking_ban;

 public:
	ModuleServicesAccount()
		: Whois::EventListener(this)
		, CTCTags::EventListener(this)
		, calleridapi(this)
		, exemptionprov(this)
		, m1(this, "reginvite", 'R')
		, m2(this, "regmoderated", 'M')
		, m3(this, "regdeaf", 'R')
		, m4(this)
		, m5(this)
		, accountname(this)
		, checking_ban(false)
	{
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["EXTBAN"].push_back('R');
		tokens["EXTBAN"].push_back('U');
	}

	/* <- :twisted.oscnet.org 330 w00t2 w00t2 w00t :is logged in as */
	void OnWhois(Whois::Context& whois) CXX11_OVERRIDE
	{
		std::string* account = accountname.get(whois.GetTarget());

		if (account)
		{
			whois.SendLine(RPL_WHOISACCOUNT, *account, "is logged in as");
		}

		if (whois.GetTarget()->IsModeSet(m5))
		{
			/* user is registered */
			whois.SendLine(RPL_WHOISREGNICK, "is a registered nick");
		}
	}

	void OnUserPostNick(User* user, const std::string &oldnick) CXX11_OVERRIDE
	{
		/* On nickchange, if they have +r, remove it */
		if ((user->IsModeSet(m5)) && (ServerInstance->FindNickOnly(oldnick) != user))
			m5.RemoveMode(user);
	}

	ModResult HandleMessage(User* user, const MessageTarget& target)
	{
		if (!IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		std::string *account = accountname.get(user);
		bool is_registered = account && !account->empty();

		switch (target.type)
		{
			case MessageTarget::TYPE_CHANNEL:
			{
				Channel* targchan = target.Get<Channel>();

				if (!targchan->IsModeSet(m2) || is_registered)
					return MOD_RES_PASSTHRU;

				if (CheckExemption::Call(exemptionprov, user, targchan, "regmoderated") == MOD_RES_ALLOW)
					return MOD_RES_PASSTHRU;

				// User is messaging a +M channel and is not registered or exempt.
				user->WriteNumeric(ERR_NEEDREGGEDNICK, targchan->name, "You need to be identified to a registered account to message this channel");
				return MOD_RES_DENY;
				break;
			}
			case MessageTarget::TYPE_USER:
			{
				User* targuser = target.Get<User>();
				if (!targuser->IsModeSet(m3)  || is_registered)
					return MOD_RES_PASSTHRU;

				if (calleridapi && calleridapi->IsOnAcceptList(user, targuser))
					return MOD_RES_PASSTHRU;

				// User is messaging a +R user and is not registered or on an accept list.
				user->WriteNumeric(ERR_NEEDREGGEDNICK, targuser->nick, "You need to be identified to a registered account to message this user");
				return MOD_RES_DENY;
				break;
			}
			case MessageTarget::TYPE_SERVER:
				break;
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreMessage(User* user, const MessageTarget& target, MessageDetails& details) CXX11_OVERRIDE
	{
		return HandleMessage(user, target);
	}

	ModResult OnUserPreTagMessage(User* user, const MessageTarget& target, CTCTags::TagMessageDetails& details) CXX11_OVERRIDE
	{
		return HandleMessage(user, target);
	}

	ModResult OnCheckBan(User* user, Channel* chan, const std::string& mask) CXX11_OVERRIDE
	{
		if (checking_ban)
			return MOD_RES_PASSTHRU;

		if ((mask.length() > 2) && (mask[1] == ':'))
		{
			if (mask[0] == 'R')
			{
				std::string *account = accountname.get(user);
				if (account && InspIRCd::Match(*account, mask.substr(2)))
					return MOD_RES_DENY;
			}
			else if (mask[0] == 'U')
			{
				std::string *account = accountname.get(user);
				/* If the user is registered we don't care. */
				if (account)
					return MOD_RES_PASSTHRU;

				/* If we made it this far we know the user isn't registered
					so just deny if it matches */
				checking_ban = true;
				bool result = chan->CheckBan(user, mask.substr(2));
				checking_ban = false;

				if (result)
					return MOD_RES_DENY;
			}
		}

		/* If we made it this far then the ban wasn't an ExtBan
			or the user we were checking for didn't match either ExtBan */
		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven) CXX11_OVERRIDE
	{
		std::string *account = accountname.get(user);
		bool is_registered = account && !account->empty();

		if (chan)
		{
			if (chan->IsModeSet(m1))
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

	ModResult OnSetConnectClass(LocalUser* user, ConnectClass* myclass) CXX11_OVERRIDE
	{
		if (myclass->config->getBool("requireaccount") && !accountname.get(user))
			return MOD_RES_DENY;
		return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support for ircu-style services accounts, including channel mode +R, etc", VF_OPTCOMMON|VF_VENDOR);
	}
};

MODULE_INIT(ModuleServicesAccount)
