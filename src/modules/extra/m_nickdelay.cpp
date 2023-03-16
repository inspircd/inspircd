/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
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

/// $ModAuthor: Attila Molnar
/// $ModAuthorMail: attilamolnar@hush.com
/// $ModConfig: <nickdelay delay="10" hint="true">
/// $ModDepends: core 3
/// $ModDesc: Enforces a delay between nick changes per user
// If you want opers to be exempt, add the priv 'users/ignore-nickdelay' to their oper class.


#include "inspircd.h"

class ModuleNickDelay : public Module
{
	LocalIntExt lastchanged;
	unsigned int delay;
	bool hint;

 public:
	ModuleNickDelay()
		: lastchanged("nickdelay", ExtensionItem::EXT_USER, this)
	{
	}

	void OnUserPostNick(User* user, const std::string& oldnick) CXX11_OVERRIDE
	{
		// Ignore remote users and nick changes to uuid
		if ((IS_LOCAL(user)) && (user->nick != user->uuid))
			lastchanged.set(user, ServerInstance->Time());
	}

	ModResult OnUserPreNick(LocalUser* user, const std::string& newnick) CXX11_OVERRIDE
	{
		if (user->HasPrivPermission("users/ignore-nickdelay"))
			return MOD_RES_PASSTHRU;

		time_t lastchange = lastchanged.get(user);
		time_t wait = lastchange + delay - ServerInstance->Time();
		if (wait > 0)
		{
			if (hint)
			{
				user->WriteNumeric(ERR_CANTCHANGENICK, user->nick,
					InspIRCd::Format("You cannot change your nickname (try again in %s)",
					InspIRCd::DurationString(wait).c_str()));
			}
			else
			{
				user->WriteNumeric(ERR_CANTCHANGENICK, user->nick,
					"You cannot change your nickname (try again later)");
			}

			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	void ReadConfig(ConfigStatus&) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("nickdelay");
		delay = tag->getUInt("delay", 10, 1);
		hint = tag->getBool("hint", true);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Enforces a delay between nick changes per user");
	}
};

MODULE_INIT(ModuleNickDelay)
