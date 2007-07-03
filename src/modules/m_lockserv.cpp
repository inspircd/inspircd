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
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Allows locking of the server to stop all incoming connections till unlocked again */

/** Adds numerics
 * 988 <nick> <servername> :Closed for new connections
 * 989 <nick> <servername> :Open for new connections
*/


class cmd_lockserv : public command_t
{
private:
	bool& locked;

public:
	cmd_lockserv (InspIRCd* Instance, bool &lock)
	: command_t(Instance, "LOCKSERV", 'o', 0), locked(lock)
	{
		this->source = "m_lockserv.so";
		syntax.clear();
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		locked = true;
		user->WriteServ("988 %s %s :Closed for new connections", user->nick, user->server);
		ServerInstance->WriteOpers("*** Oper %s used LOCKSERV to temporarily close for new connections", user->nick);
		/* Dont send to the network */
		return CMD_LOCALONLY;
	}
};

class cmd_unlockserv : public command_t
{
private:
	bool& locked;

public:
	cmd_unlockserv (InspIRCd* Instance, bool &lock)
	: command_t(Instance, "UNLOCKSERV", 'o', 0), locked(lock)
	{
		this->source = "m_lockserv.so";
		syntax.clear();
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
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
	cmd_lockserv* lockcommand;
	cmd_unlockserv* unlockcommand;

	virtual void ResetLocked()
	{
		locked = false;
	}

public:
	ModuleLockserv(InspIRCd* Me) : Module(Me)
	{
		ResetLocked();
		lockcommand = new cmd_lockserv(ServerInstance, locked);
		ServerInstance->AddCommand(lockcommand);

		unlockcommand = new cmd_unlockserv(ServerInstance, locked);
		ServerInstance->AddCommand(unlockcommand);
	}

	virtual ~ModuleLockserv()
	{
	}

	void Implements(char* List)
	{
		List[I_OnUserRegister] = List[I_OnRehash] = List[I_OnCheckReady] = 1;
	}

	virtual void OnRehash(userrec* user, const std::string &parameter)
	{
		ResetLocked();
	}

	virtual int OnUserRegister(userrec* user)
	{
		if (locked)
		{
			userrec::QuitUser(ServerInstance, user, "Server is temporarily closed. Please try again later.");
			return 1;
		}
		return 0;
	}

	virtual bool OnCheckReady(userrec* user)
	{
		return !locked;
	}

	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 1, VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleLockserv)
