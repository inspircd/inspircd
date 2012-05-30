/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
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


/* $ModDesc: Provides support for modes related to accounts. */

#include "inspircd.h"
#include "account.h"

/** Channel mode +R - unidentified users cannot join
 */
class AChannel_R : public SimpleChannelModeHandler
{
 public:
	AChannel_R(Module* Creator) : SimpleChannelModeHandler(Creator, "reginvite", 'R') { fixed_letter = false; }
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
	AChannel_M(Module* Creator) : SimpleChannelModeHandler(Creator, "regmoderated", 'M') { fixed_letter = false; }
};

class ModuleAccountModes : public Module
{
	AChannel_R chanR;
	AChannel_M chanM;
	AUser_R m3;
	dynamic_reference<AccountProvider> account;
 public:
	ModuleAccountModes() : chanR(this), chanM(this), m3(this), account("account") {}

	void init()
	{
		ServerInstance->Modules->AddService(chanR);
		ServerInstance->Modules->AddService(chanM);
		ServerInstance->Modules->AddService(m3);
		Implementation eventlist[] = {
			I_OnUserPreMessage, I_OnUserPreNotice, I_OnCheckJoin, I_OnCheckBan, I_On005Numeric
		};
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void On005Numeric(std::string &t)
	{
		ServerInstance->AddExtBanChar('r');
	}

	ModResult OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (!IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		if (target_type == TYPE_CHANNEL)
		{
			Channel* c = (Channel*)dest;
			ModResult res = ServerInstance->CheckExemption(user,c,"regmoderated");

			if (c->IsModeSet(&chanM) && (!account || !account->IsRegistered(user)) && res != MOD_RES_ALLOW)
			{
				// user messaging a +M channel and is not registered
				user->WriteNumeric(477, ""+std::string(user->nick)+" "+std::string(c->name)+" :You need to be identified to a registered account to message this channel");
				return MOD_RES_DENY;
			}
		}
		else if (target_type == TYPE_USER)
		{
			User* u = (User*)dest;

			if (u->IsModeSet('R') && (!account || !account->IsRegistered(user)))
			{
				// user messaging a +R user and is not registered
				user->WriteNumeric(477, ""+ user->nick +" "+ u->nick +" :You need to be identified to a registered account to message this user");
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	// XXX: Does this belong in m_services_account?
	ModResult OnCheckBan(User* user, Channel* chan, const std::string& mask)
	{
		if (mask[0] == 'r' && mask[1] == ':')
		{
			irc::string acctname = account ? account->GetAccountName(user) : "";
			if (acctname == mask.substr(2))
				return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user, dest, target_type, text, status, exempt_list);
	}

	void OnCheckJoin(ChannelPermissionData& join)
	{
		if (!join.chan || join.result != MOD_RES_PASSTHRU)
			return;

		if (join.chan->IsModeSet(&chanR) && (!account || !account->IsRegistered(join.source)))
		{
			// joining a +R channel and not identified
			join.ErrorNumeric(477, "%s :You need to be identified to a registered account to join this channel", join.chan->name.c_str());
			join.result = MOD_RES_DENY;
		}
	}

	Version GetVersion()
	{
		return Version("Provides support for modes related to accounts.", VF_VENDOR);
	}
};

MODULE_INIT(ModuleAccountModes)
