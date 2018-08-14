/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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

class ModuleQuietBan : public Module
{
 public:
	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Implements extban +b m: - mute bans",VF_OPTCOMMON|VF_VENDOR);
	}

	ModResult OnUserPreMessage(User* user, const MessageTarget& target, MessageDetails& details) CXX11_OVERRIDE
	{
		if (!IS_LOCAL(user) || target.type != MessageTarget::TYPE_CHANNEL)
			return MOD_RES_PASSTHRU;

		Channel* chan = target.Get<Channel>();
		if (chan->GetExtBanStatus(user, 'm') == MOD_RES_DENY && chan->GetPrefixValue(user) < VOICE_VALUE)
		{
			bool notifyuser = ServerInstance->Config->ConfValue("muteban")->getBool("notifyuser", true);
			if (!notifyuser)
			{
				details.echo_original = true;
				return MOD_RES_DENY;
			}

			user->WriteNumeric(ERR_CANNOTSENDTOCHAN, chan->name, "Cannot send to channel (you're muted)");
			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["EXTBAN"].push_back('m');
	}
};

MODULE_INIT(ModuleQuietBan)
