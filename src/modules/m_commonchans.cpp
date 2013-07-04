/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
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

/** Handles user mode +c
 */
class PrivacyMode : public SimpleUserModeHandler
{
 public:
	PrivacyMode(Module* Creator) : SimpleUserModeHandler(Creator, "deaf_commonchan", 'c') { }
};

class ModulePrivacyMode : public Module
{
	PrivacyMode pm;
 public:
	ModulePrivacyMode() : pm(this)
	{
	}

	void init() CXX11_OVERRIDE
	{
		ServerInstance->Modules->AddService(pm);
		ServerInstance->Modules->Attach(I_OnUserPreMessage, this);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Adds user mode +c, which if set, users must be on a common channel with you to private message you", VF_VENDOR);
	}

	ModResult OnUserPreMessage(User* user, void* dest, int target_type, std::string& text, char status, CUList& exempt_list, MessageType msgtype) CXX11_OVERRIDE
	{
		if (target_type == TYPE_USER)
		{
			User* t = (User*)dest;
			if (!user->IsOper() && (t->IsModeSet(pm)) && (!ServerInstance->ULine(user->server)) && !user->SharesChannelWith(t))
			{
				user->WriteNumeric(ERR_CANTSENDTOUSER, "%s %s :You are not permitted to send private messages to this user (+c set)", user->nick.c_str(), t->nick.c_str());
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModulePrivacyMode)
