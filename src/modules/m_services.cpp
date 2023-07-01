/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022 Sadie Powell <sadie@witchery.services>
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

enum
{
	// From UnrealIRCd.
	ERR_KILLDENY = 485
};

class RegisteredChannel final
	: public SimpleChannelMode
{
public:
	RegisteredChannel(Module* Creator)
		: SimpleChannelMode(Creator, "c_registered", 'r')
	{
		if (ServerInstance->Config->ConfValue("services")->getBool("disablemodes"))
			DisableAutoRegister();
	}

	bool OnModeChange(User* source, User* dest, Channel* channel, Modes::Change& change) override
	{
		if (IS_LOCAL(source))
		{
			source->WriteNumeric(ERR_NOPRIVILEGES, "Only a server may modify the +r channel mode");
			return false;
		}

		return SimpleChannelMode::OnModeChange(source, dest, channel, change);
	}
};

class RegisteredUser final
	: public SimpleUserMode
{

public:
	RegisteredUser(Module* Creator)
		: SimpleUserMode(Creator, "u_registered", 'r')
	{
		if (ServerInstance->Config->ConfValue("services")->getBool("disablemodes"))
			DisableAutoRegister();
	}

	bool OnModeChange(User* source, User* dest, Channel* channel, Modes::Change& change) override
	{
		if (IS_LOCAL(source))
		{
			source->WriteNumeric(ERR_NOPRIVILEGES, "Only a server may modify the +r user mode");
			return false;
		}

		return SimpleUserMode::OnModeChange(source, dest, channel, change);
	}
};

class ServProtect final
	: public SimpleUserMode
{
public:
	ServProtect(Module* Creator)
		: SimpleUserMode(Creator, "servprotect", 'k', true)
	{
	}

	bool OnModeChange(User* source, User* dest, Channel* channel, Modes::Change& change) override
	{
		// As this mode is only intended for use by pseudoclients the only way
		// to set it is by introducing a user with it.
		return false;
	}
};

class ModuleServices final
	: public Module
{
private:
	RegisteredChannel registeredcmode;
	RegisteredUser registeredumode;
	ServProtect servprotectmode;

public:
	ModuleServices()
		: Module(VF_VENDOR, "Provides support for integrating with a services server.")
		, registeredcmode(this)
		, registeredumode(this)
		, servprotectmode(this)
	{
	}

	ModResult OnKill(User* source, User* dest, const std::string& reason) override
	{
		if (!source)
			return MOD_RES_PASSTHRU;

		if (dest->IsModeSet(servprotectmode))
		{
			source->WriteNumeric(ERR_KILLDENY, INSP_FORMAT("You are not permitted to kill {} services!", ServerInstance->Config->Network));
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnRawMode(User* user, Channel* chan, const Modes::Change& change) override
	{
		if (!IS_LOCAL(user) || change.adding || change.param.empty())
			return MOD_RES_PASSTHRU; // We only care about local users removing prefix modes.

		const PrefixMode* const pm = change.mh->IsPrefixMode();
		if (!pm)
			return MOD_RES_PASSTHRU; // Mode is not a prefix mode.

		auto* target = ServerInstance->Users.Find(change.param);
		if (!target)
			return MOD_RES_PASSTHRU; // Target does not exist.

		Membership* memb = chan->GetUser(target);
		if (!memb || !memb->HasMode(pm))
			return MOD_RES_PASSTHRU; // Target does not have the mode.

		if (target->IsModeSet(servprotectmode))
		{
			user->WriteNumeric(ERR_RESTRICTED, chan->name, INSP_FORMAT("You are not permitted to remove privileges from {} services!", ServerInstance->Config->Network));
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreKick(User* source, Membership* memb, const std::string& reason) override
	{
		if (memb->user->IsModeSet(servprotectmode))
		{
			source->WriteNumeric(ERR_RESTRICTED, memb->chan->name, INSP_FORMAT("You are not permitted to kick {} services!", ServerInstance->Config->Network));
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	void OnUserPostNick(User* user, const std::string& oldnick) override
	{
		if (user->IsModeSet(registeredumode) && irc::equals(oldnick, user->nick))
			registeredumode.RemoveMode(user);
	}
};

MODULE_INIT(ModuleServices)
