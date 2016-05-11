/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2006-2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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

/** Adds numerics
 * 988 <nick> <servername> :Closed for new connections
 * 989 <nick> <servername> :Open for new connections
 */

class CommandLockserv : public Command
{
	std::string& locked;

 public:
	CommandLockserv(Module* Creator, std::string& lock) : Command(Creator, "LOCKSERV", 0, 1), locked(lock)
	{
		allow_empty_last_param = false;
		flags_needed = 'o';
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		if (!locked.empty())
		{
			user->WriteNotice("The server is already locked.");
			return CMD_FAILURE;
		}

		locked = parameters.empty() ? "Server is temporarily closed. Please try again later." : parameters[0];
		user->WriteNumeric(988, user->server->GetName(), "Closed for new connections");
		ServerInstance->SNO->WriteGlobalSno('a', "Oper %s used LOCKSERV to temporarily disallow new connections", user->nick.c_str());
		return CMD_SUCCESS;
	}
};

class CommandUnlockserv : public Command
{
	std::string& locked;

 public:
	CommandUnlockserv(Module* Creator, std::string& lock) : Command(Creator, "UNLOCKSERV", 0), locked(lock)
	{
		flags_needed = 'o';
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		if (locked.empty())
		{
			user->WriteNotice("The server isn't locked.");
			return CMD_FAILURE;
		}

		locked.clear();
		user->WriteNumeric(989, user->server->GetName(), "Open for new connections");
		ServerInstance->SNO->WriteGlobalSno('a', "Oper %s used UNLOCKSERV to allow new connections", user->nick.c_str());
		return CMD_SUCCESS;
	}
};

class ModuleLockserv : public Module
{
	std::string locked;
	CommandLockserv lockcommand;
	CommandUnlockserv unlockcommand;

 public:
	ModuleLockserv() : lockcommand(this, locked), unlockcommand(this, locked)
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		// Emergency way to unlock
		if (!status.srcuser)
			locked.clear();
	}

	ModResult OnUserRegister(LocalUser* user) CXX11_OVERRIDE
	{
		if (!locked.empty())
		{
			ServerInstance->Users->QuitUser(user, locked);
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnCheckReady(LocalUser* user) CXX11_OVERRIDE
	{
		return !locked.empty() ? MOD_RES_DENY : MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Allows locking of the server to stop all incoming connections until unlocked again", VF_VENDOR);
	}
};

MODULE_INIT(ModuleLockserv)
