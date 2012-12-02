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

/* $ModDesc: Allows locking of the server to stop all incoming connections till unlocked again */

/** Adds numerics
 * 988 <nick> <servername> :Closed for new connections
 * 989 <nick> <servername> :Open for new connections
*/


class CommandLockserv : public Command
{
	bool& locked;
public:
	CommandLockserv(Module* Creator, bool& lock) : Command(Creator, "LOCKSERV", 0), locked(lock)
	{
		flags_needed = 'o';
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		if (locked)
		{
			user->WriteServ("NOTICE %s :The server is already locked.", user->nick.c_str());
			return CMD_FAILURE;
		}

		locked = true;
		user->WriteNumeric(988, "%s %s :Closed for new connections", user->nick.c_str(), user->server.c_str());
		ServerInstance->SNO->WriteGlobalSno('a', "Oper %s used LOCKSERV to temporarily disallow new connections", user->nick.c_str());
		return CMD_SUCCESS;
	}
};

class CommandUnlockserv : public Command
{
private:
	bool& locked;

public:
	CommandUnlockserv(Module* Creator, bool &lock) : Command(Creator, "UNLOCKSERV", 0), locked(lock)
	{
		flags_needed = 'o';
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		if (!locked)
		{
			user->WriteServ("NOTICE %s :The server isn't locked.", user->nick.c_str());
			return CMD_FAILURE;
		}

		locked = false;
		user->WriteNumeric(989, "%s %s :Open for new connections", user->nick.c_str(), user->server.c_str());
		ServerInstance->SNO->WriteGlobalSno('a', "Oper %s used UNLOCKSERV to allow new connections", user->nick.c_str());
		return CMD_SUCCESS;
	}
};

class ModuleLockserv : public Module
{
private:
	bool locked;
	CommandLockserv lockcommand;
	CommandUnlockserv unlockcommand;

public:
	ModuleLockserv() : lockcommand(this, locked), unlockcommand(this, locked)
	{
	}

	void init()
	{
		locked = false;
		ServerInstance->Modules->AddService(lockcommand);
		ServerInstance->Modules->AddService(unlockcommand);
		Implementation eventlist[] = { I_OnUserRegister, I_OnRehash, I_OnCheckReady };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual ~ModuleLockserv()
	{
	}


	virtual void OnRehash(User* user)
	{
		// Emergency way to unlock
		if (!user) locked = false;
	}

	virtual ModResult OnUserRegister(LocalUser* user)
	{
		if (locked)
		{
			ServerInstance->Users->QuitUser(user, "Server is temporarily closed. Please try again later.");
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	virtual ModResult OnCheckReady(LocalUser* user)
	{
		return locked ? MOD_RES_DENY : MOD_RES_PASSTHRU;
	}

	virtual Version GetVersion()
	{
		return Version("Allows locking of the server to stop all incoming connections until unlocked again", VF_VENDOR);
	}
};

MODULE_INIT(ModuleLockserv)
