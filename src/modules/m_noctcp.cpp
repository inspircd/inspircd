/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2004, 2006 Craig Edwards <craigedwards@brainbox.cc>
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
#include "modules/exemption.h"

class NoCTCPUser : public SimpleUserModeHandler
{
public:
	NoCTCPUser(Module* Creator)
		: SimpleUserModeHandler(Creator, "u_noctcp", 'T')
	{
		if (!ServerInstance->Config->ConfValue("noctcp")->getBool("enableumode"))
			DisableAutoRegister();
	}
};

class ModuleNoCTCP : public Module
{
	CheckExemption::EventProvider exemptionprov;
	SimpleChannelModeHandler nc;
	NoCTCPUser ncu;

 public:
	ModuleNoCTCP()
		: exemptionprov(this)
		, nc(this, "noctcp", 'C')
		, ncu(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides user mode +T and channel mode +C to block CTCPs", VF_VENDOR);
	}

	ModResult OnUserPreMessage(User* user, const MessageTarget& target, MessageDetails& details) CXX11_OVERRIDE
	{
		if (!IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		std::string ctcpname;
		if (!details.IsCTCP(ctcpname) || irc::equals(ctcpname, "ACTION"))
			return MOD_RES_PASSTHRU;

		switch (target.type)
		{
			case MessageTarget::TYPE_CHANNEL:
			{
				if (user->HasPrivPermission("channels/ignore-noctcp"))
					return MOD_RES_PASSTHRU;

				Channel* c = target.Get<Channel>();
				ModResult res = CheckExemption::Call(exemptionprov, user, c, "noctcp");
				if (res == MOD_RES_ALLOW)
					return MOD_RES_PASSTHRU;

				if (!c->GetExtBanStatus(user, 'C').check(!c->IsModeSet(nc)))
				{
					user->WriteNumeric(ERR_CANNOTSENDTOCHAN, c->name, "Can't send CTCP to channel (+C is set)");
					return MOD_RES_DENY;
				}
				break;
			}
			case MessageTarget::TYPE_USER:
			{
				if (user->HasPrivPermission("users/ignore-noctcp"))
					return MOD_RES_PASSTHRU;

				User* u = target.Get<User>();
				if (u->IsModeSet(ncu))
				{
					user->WriteNumeric(ERR_CANTSENDTOUSER, u->nick, "Can't send CTCP to user (+T is set)");
					return MOD_RES_DENY;
				}
				break;
			}
			case MessageTarget::TYPE_SERVER:
				break;
		}
		return MOD_RES_PASSTHRU;
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["EXTBAN"].push_back('C');
	}
};

MODULE_INIT(ModuleNoCTCP)
