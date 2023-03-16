/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Jansteen <pliantcom@yandex.com>
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

/// $ModDesc: Provides support for blocking DCC transfers
/// $ModAuthor: Jansteen
/// $ModAuthorMail: pliantcom@yandex.com
/// $ModConfig: <dccblock users="yes" channels="yes">
/// $ModDepends: core 3

/* Documentation
   This module is used to completely block DCC from being used on your
   IRC network in all cases.  Previous work-arounds to accomplish this
   included configuring m_dccallow to block DCC by default and adding an
   extra clause to disable DCCALLOW for non-operators, but there are
   some cases where it is best to make sure DCC is off for everyone,
   operators included.  This module does this simply and without extra
   configuration required.

   Note: You should not load m_dccallow.so simultaneously to m_dccblock.so
   because it will do nothing useful.  It wouldn't prevent this module from
   blocking DCC however.
*/

class ModuleDCCBlock : public Module
{
 private:
	bool users;
	bool channels;

 public:
	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("dccblock");
		users = tag->getBool("users", true);
		channels = tag->getBool("channels", true);
	}

	ModResult OnUserPreMessage(User* user, const MessageTarget& target, MessageDetails& details) CXX11_OVERRIDE
	{
		if (!IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		std::string ctcptype;
		if (details.IsCTCP(ctcptype) && irc::equals(ctcptype, "DCC"))
		{
			if (target.type == MessageTarget::TYPE_USER && users)
			{
				user->WriteNumeric(Numerics::CannotSendTo(target.Get<User>(), "You cannot send DCCs to users."));
				return MOD_RES_DENY;
			}

			if (target.type == MessageTarget::TYPE_CHANNEL && channels)
			{
				user->WriteNumeric(Numerics::CannotSendTo(target.Get<Channel>(), "You cannot send DCCs to channels."));
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support for blocking DCC transfers");
	}
};

MODULE_INIT(ModuleDCCBlock)
