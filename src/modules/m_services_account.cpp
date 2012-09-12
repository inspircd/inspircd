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


/* $ModDesc: Provides support for ircu-style services accounts, including chmode +R, etc. */

#include "inspircd.h"
#include "account.h"

/** Channel mode +r - mark a channel as identified
 */
class Channel_r : public ModeHandler
{
 public:
	Channel_r(Module* Creator) : ModeHandler(Creator, "c_registered", 'r', PARAM_NONE, MODETYPE_CHANNEL) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		// only a u-lined server may add or remove the +r mode.
		if (!IS_LOCAL(source) || ServerInstance->ULine(source->server))
		{
			// Only change the mode if it's not redundant
			if ((adding != channel->IsModeSet('r')))
			{
				channel->SetMode('r',adding);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			source->WriteNumeric(500, "%s :Only a server may modify the +r channel mode", source->nick.c_str());
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
		if (!IS_LOCAL(source) || ServerInstance->ULine(source->server))
		{
			if ((adding != dest->IsModeSet('r')))
			{
				dest->SetMode('r',adding);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			source->WriteNumeric(500, "%s :Only a server may modify the +r user mode", source->nick.c_str());
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

class ModuleServicesAccount : public Module
{
	AChannel_R m1;
	AChannel_M m2;
	AUser_R m3;
	Channel_r m4;
	User_r m5;
	AccountExtItem accountname;
 public:
	ModuleServicesAccount() : m1(this), m2(this), m3(this), m4(this), m5(this),
		accountname("accountname", this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(m1);
		ServerInstance->Modules->AddService(m2);
		ServerInstance->Modules->AddService(m3);
		ServerInstance->Modules->AddService(m4);
		ServerInstance->Modules->AddService(m5);
		ServerInstance->Modules->AddService(accountname);
		Implementation eventlist[] = { I_OnWhois, I_OnUserPreMessage, I_OnUserPreNotice, I_OnUserPreJoin, I_OnCheckBan,
			I_OnDecodeMetaData, I_On005Numeric, I_OnUserPostNick, I_OnSetConnectClass };

		ServerInstance->Modules->Attach(eventlist, this, 9);
	}

	void On005Numeric(std::string &t)
	{
		ServerInstance->AddExtBanChar('R');
		ServerInstance->AddExtBanChar('U');
	}

	/* <- :twisted.oscnet.org 330 w00t2 w00t2 w00t :is logged in as */
	void OnWhois(User* source, User* dest)
	{
		std::string *account = accountname.get(dest);

		if (account)
		{
			ServerInstance->SendWhoisLine(source, dest, 330, "%s %s %s :is logged in as", source->nick.c_str(), dest->nick.c_str(), account->c_str());
		}

		if (dest->IsModeSet('r'))
		{
			/* user is registered */
			ServerInstance->SendWhoisLine(source, dest, 307, "%s %s :is a registered nick", source->nick.c_str(), dest->nick.c_str());
		}
	}

	void OnUserPostNick(User* user, const std::string &oldnick)
	{
		/* On nickchange, if they have +r, remove it */
		if (user->IsModeSet('r') && assign(user->nick) != oldnick)
		{
			std::vector<std::string> modechange;
			modechange.push_back(user->nick);
			modechange.push_back("-r");
			ServerInstance->SendMode(modechange, ServerInstance->FakeClient);
		}
	}

	ModResult OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (!IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		std::string *account = accountname.get(user);
		bool is_registered = account && !account->empty();

		if ((ServerInstance->ULine(user->nick.c_str())) || (ServerInstance->ULine(user->server)))
		{
			// user is ulined, can speak regardless
			return MOD_RES_PASSTHRU;
		}

		if (target_type == TYPE_CHANNEL)
		{
			Channel* c = (Channel*)dest;
			ModResult res = ServerInstance->OnCheckExemption(user,c,"regmoderated");

			if (c->IsModeSet('M') && !is_registered && res != MOD_RES_ALLOW)
			{
				// user messaging a +M channel and is not registered
				user->WriteNumeric(477, ""+std::string(user->nick)+" "+std::string(c->name)+" :You need to be identified to a registered account to message this channel");
				return MOD_RES_DENY;
			}
		}
		else if (target_type == TYPE_USER)
		{
			User* u = (User*)dest;

			if (u->IsModeSet('R') && !is_registered)
			{
				// user messaging a +R user and is not registered
				user->WriteNumeric(477, ""+ user->nick +" "+ u->nick +" :You need to be identified to a registered account to message this user");
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnCheckBan(User* user, Channel* chan, const std::string& mask)
	{
		if (mask[1] == ':')
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
				if (chan->GetExtBanStatus(user, 'U') == MOD_RES_DENY)
					return MOD_RES_DENY;
			}
		}

		/* If we made it this far then the ban wasn't an ExtBan
			or the user we were checking for didn't match either ExtBan */
		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user, dest, target_type, text, status, exempt_list);
	}

	ModResult OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		if (!IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		std::string *account = accountname.get(user);
		bool is_registered = account && !account->empty();

		if (chan)
		{
			if ((ServerInstance->ULine(user->nick.c_str())) || (ServerInstance->ULine(user->server)))
			{
				// user is ulined, won't be stopped from joining
				return MOD_RES_PASSTHRU;
			}

			if (chan->IsModeSet('R'))
			{
				if (!is_registered)
				{
					// joining a +R channel and not identified
					user->WriteNumeric(477, user->nick + " " + chan->name + " :You need to be identified to a registered account to join this channel");
					return MOD_RES_DENY;
				}
			}
		}
		return MOD_RES_PASSTHRU;
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
		User* dest = dynamic_cast<User*>(target);
		// check if its our metadata key, and its associated with a user
		if (dest && (extname == "accountname"))
		{
			std::string *account = accountname.get(dest);
			if (account && !account->empty())
			{
				trim(*account);

				if (IS_LOCAL(dest))
					dest->WriteNumeric(900, "%s %s %s :You are now logged in as %s",
						dest->nick.c_str(), dest->GetFullHost().c_str(), account->c_str(), account->c_str());

				AccountEvent(this, dest, *account).Send();
			}
			else
			{
				AccountEvent(this, dest, "").Send();
			}
		}
	}

	ModResult OnSetConnectClass(LocalUser* user, ConnectClass* myclass)
	{
		if (myclass->config->getBool("requireaccount") && !accountname.get(user))
			return MOD_RES_DENY;
		return MOD_RES_PASSTHRU;
	}

	Version GetVersion()
	{
		return Version("Provides support for ircu-style services accounts, including chmode +R, etc.",VF_OPTCOMMON|VF_VENDOR);
	}
};

MODULE_INIT(ModuleServicesAccount)
