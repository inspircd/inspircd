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

class ModuleServices final
	: public Module
{
private:
	Account::API accountapi;
	RegisteredChannel chanmode;
	RegisteredUser usermode;

public:
	ModuleServices()
		: Module(VF_VENDOR, "Provides support for integrating with a services server.")
		, accountapi(this)
		, chanmode(this)
		, usermode(this)
	{
	}

	void OnUserPostNick(User* user, const std::string& oldnick) override
	{
		if (user->IsModeSet(usermode) && irc::equals(oldnick, user->nick))
			usermode.RemoveMode(user);
	}
};

MODULE_INIT(ModuleServices)
