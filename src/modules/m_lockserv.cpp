/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
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
		flags_needed = 'o'; syntax.clear();
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		locked = true;
		user->WriteNumeric(988, "%s %s :Closed for new connections", user->nick.c_str(), user->server);
		ServerInstance->SNO->WriteGlobalSno('a', "Oper %s used LOCKSERV to temporarily close for new connections", user->nick.c_str());
		/* Dont send to the network */
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
		flags_needed = 'o'; syntax.clear();
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		locked = false;
		user->WriteNumeric(989, "%s %s :Open for new connections", user->nick.c_str(), user->server);
		ServerInstance->SNO->WriteGlobalSno('a', "Oper %s used UNLOCKSERV to allow for new connections", user->nick.c_str());
		/* Dont send to the network */
		return CMD_SUCCESS;
	}
};

class ModuleLockserv : public Module
{
private:
	bool locked;
	CommandLockserv lockcommand;
	CommandUnlockserv unlockcommand;

	virtual void ResetLocked()
	{
		locked = false;
	}

public:
	ModuleLockserv() : lockcommand(this, locked), unlockcommand(this, locked)
	{
		ResetLocked();
		ServerInstance->AddCommand(&lockcommand);
		ServerInstance->AddCommand(&unlockcommand);
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

	virtual ModResult OnUserRegister(User* user)
	{
		if (locked)
		{
			ServerInstance->Users->QuitUser(user, "Server is temporarily closed. Please try again later.");
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	virtual ModResult OnCheckReady(User* user)
	{
		return locked ? MOD_RES_DENY : MOD_RES_PASSTHRU;
	}

	virtual Version GetVersion()
	{
		return Version("Allows locking of the server to stop all incoming connections till unlocked again", VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleLockserv)
