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
#include "modules/exemption.h"

/** Channel mode +r - mark a channel as identified
 */
class Channel_r : public ModeHandler
{
 public:
	Channel_r(Module* Creator) : ModeHandler(Creator, "c_registered", 'r', PARAM_NONE, MODETYPE_CHANNEL) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		// only a u-lined server may add or remove the +r mode.
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
			source->WriteNumeric(500, "Only a server may modify the +r channel mode");
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

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
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
			source->WriteNumeric(500, "Only a server may modify the +r user mode");
		}
		return MODEACTION_DENY;
	}
};

/** Channel mode +R - unidentified users cannot join
 */
class AChannel_R : public SimpleChannelModeHandler
{
 public:
	AChannel_R(Module* Creator) : SimpleChannelModeHandler(Creator, "reginvite", 'R') { }
};

/** User mode +R - unidentified users cannot message
 */
class AUser_R : public SimpleUserModeHandler
{
 public:
	AUser_R(Module* Creator) : SimpleUserModeHandler(Creator, "regdeaf", 'R') { }
};

/** Channel mode +M - unidentified users cannot message channel
 */
class AChannel_M : public SimpleChannelModeHandler
{
 public:
	AChannel_M(Module* Creator) : SimpleChannelModeHandler(Creator, "regmoderated", 'M') { }
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

	void unserialize(SerializeFormat format, Extensible* container, const std::string& value)
	{
		User* user = static_cast<User*>(container);

		StringExtItem::unserialize(format, container, value);

		// If we are being reloaded then don't send the numeric or run the event
		if (format == FORMAT_INTERNAL)
			return;

		if (!value.empty())
		{
			// Logged in
			if (IS_LOCAL(user))
			{
				user->WriteNumeric(900, user->GetFullHost(), value, InspIRCd::Format("You are now logged in as %s", value.c_str()));
			}
		}
		// If value is empty then logged out

		FOREACH_MOD_CUSTOM(eventprov, AccountEventListener, OnAccountChange, (user, value));
	}
};

class ModuleServicesAccount : public Module, public Whois::EventListener
{
	CheckExemption::EventProvider exemptionprov;
	AChannel_R m1;
	AChannel_M m2;
	AUser_R m3;
	Channel_r m4;
	User_r m5;
	AccountExtItemImpl accountname;
	bool checking_ban;
 public:
	ModuleServicesAccount()
		: Whois::EventListener(this)
		, exemptionprov(this)
		, m1(this), m2(this), m3(this), m4(this), m5(this)
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
			whois.SendLine(330, *account, "is logged in as");
		}

		if (whois.GetTarget()->IsModeSet(m5))
		{
			/* user is registered */
			whois.SendLine(307, "is a registered nick");
		}
	}

	void OnUserPostNick(User* user, const std::string &oldnick) CXX11_OVERRIDE
	{
		/* On nickchange, if they have +r, remove it */
		if ((user->IsModeSet(m5)) && (ServerInstance->FindNickOnly(oldnick) != user))
			m5.RemoveMode(user);
	}

	ModResult OnUserPreMessage(User* user, void* dest, int target_type, std::string& text, char status, CUList& exempt_list, MessageType msgtype) CXX11_OVERRIDE
	{
		if (!IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		std::string *account = accountname.get(user);
		bool is_registered = account && !account->empty();

		if (target_type == TYPE_CHANNEL)
		{
			Channel* c = (Channel*)dest;
			ModResult res;
			FIRST_MOD_RESULT_CUSTOM(exemptionprov, CheckExemption::EventListener, OnCheckExemption, res, (user, c, "regmoderated"));

			if (c->IsModeSet(m2) && !is_registered && res != MOD_RES_ALLOW)
			{
				// user messaging a +M channel and is not registered
				user->WriteNumeric(477, c->name, "You need to be identified to a registered account to message this channel");
				return MOD_RES_DENY;
			}
		}
		else if (target_type == TYPE_USER)
		{
			User* u = (User*)dest;

			if (u->IsModeSet(m3) && !is_registered)
			{
				// user messaging a +R user and is not registered
				user->WriteNumeric(477, u->nick, "You need to be identified to a registered account to message this user");
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
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
					user->WriteNumeric(477, chan->name, "You need to be identified to a registered account to join this channel");
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
		return Version("Provides support for ircu-style services accounts, including chmode +R, etc.",VF_OPTCOMMON|VF_VENDOR);
	}
};

MODULE_INIT(ModuleServicesAccount)
