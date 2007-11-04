/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
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
private:
	bool& locked;

public:
	CommandLockserv (InspIRCd* Instance, bool &lock)
	: Command(Instance, "LOCKSERV", 'o', 0), locked(lock)
	{
		this->source = "m_lockserv.so";
		syntax.clear();
	}

	CmdResult Handle (const char** parameters, int pcnt, User *user)
	{
		locked = true;
		user->WriteServ("988 %s %s :Closed for new connections", user->nick, user->server);
		ServerInstance->WriteOpers("*** Oper %s used LOCKSERV to temporarily close for new connections", user->nick);
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
	: Command(Instance, "UNLOCKSERV", 'o', 0), locked(lock)
	{
		this->source = "m_lockserv.so";
		syntax.clear();
	}

	CmdResult Handle (const char** parameters, int pcnt, User *user)
	{
		locked = false;
		user->WriteServ("989 %s %s :Open for new connections", user->nick, user->server);
		ServerInstance->WriteOpers("*** Oper %s used UNLOCKSERV to allow for new connections", user->nick);
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


	virtual void OnRehash(User* user, const std::string &parameter)
	{
		ResetLocked();
	}

	virtual int OnUserRegister(User* user)
	{
		if (locked)
		{
			User::QuitUser(ServerInstance, user, "Server is temporarily closed. Please try again later.");
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
		return Version(1, 0, 0, 1, VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleLockserv)
