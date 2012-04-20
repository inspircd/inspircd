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
private:
	bool& locked;

public:
	CommandLockserv (InspIRCd* Instance, bool &lock)
	: Command(Instance, "LOCKSERV", "o", 0), locked(lock)
	{
		this->source = "m_lockserv.so";
		syntax.clear();
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		locked = true;
		user->WriteNumeric(988, "%s %s :Closed for new connections", user->nick.c_str(), user->server);
		ServerInstance->SNO->WriteGlobalSno('a', "Oper %s used LOCKSERV to temporarily close for new connections", user->nick.c_str());
		/* Dont send to the network */
		return CMD_LOCALONLY;
	}
};

class CommandUnlockserv : public Command
{
private:
	bool& locked;

public:
	CommandUnlockserv (InspIRCd* Instance, bool &lock)
	: Command(Instance, "UNLOCKSERV", "o", 0), locked(lock)
	{
		this->source = "m_lockserv.so";
		syntax.clear();
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		locked = false;
		user->WriteNumeric(989, "%s %s :Open for new connections", user->nick.c_str(), user->server);
		ServerInstance->SNO->WriteGlobalSno('a', "Oper %s used UNLOCKSERV to allow for new connections", user->nick.c_str());
		/* Dont send to the network */
		return CMD_LOCALONLY;
	}
};

class ModuleLockserv : public Module
{
private:
	bool locked;
	CommandLockserv* lockcommand;
	CommandUnlockserv* unlockcommand;

	virtual void ResetLocked()
	{
		locked = false;
	}

public:
	ModuleLockserv(InspIRCd* Me) : Module(Me)
	{
		ResetLocked();
		lockcommand = new CommandLockserv(ServerInstance, locked);
		ServerInstance->AddCommand(lockcommand);

		unlockcommand = new CommandUnlockserv(ServerInstance, locked);
		ServerInstance->AddCommand(unlockcommand);
		Implementation eventlist[] = { I_OnUserRegister, I_OnRehash, I_OnCheckReady };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	virtual ~ModuleLockserv()
	{
	}


	virtual void OnRehash(User* user)
	{
		ResetLocked();
	}

	virtual int OnUserRegister(User* user)
	{
		if (locked)
		{
			ServerInstance->Users->QuitUser(user, "Server is temporarily closed. Please try again later.");
			return 1;
		}
		return 0;
	}

	virtual bool OnCheckReady(User* user)
	{
		return !locked;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleLockserv)
