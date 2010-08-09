/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
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
		std::string param = parameters.size() ? parameters[0] : "";

		if (param.find_first_of("*.") != std::string::npos)
		{
			// lock of servers by server name (with wildcard)
			if (!InspIRCd::Match(ServerInstance->Config->ServerName, parameters[0]))
			{
				// Doesn't match us.
				return CMD_SUCCESS;
			}
		}
		locked = true;
		user->WriteNumeric(988, "%s %s :Closed for new connections", user->nick.c_str(), user->server.c_str());
		ServerInstance->SNO->WriteGlobalSno('a', "Oper %s used LOCKSERV to temporarily close for new connections", user->nick.c_str());
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		if (parameters.size() > 0 && parameters[0].find('*') != std::string::npos)
			return ROUTE_OPT_BCAST;
		if (parameters.size() > 0 && parameters[0].find('.') != std::string::npos)
			return ROUTE_OPT_UCAST(parameters[0]);
		return ROUTE_LOCALONLY;
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
		std::string param = parameters.size() ? parameters[0] : "";

		if (param.find_first_of("*.") != std::string::npos)
		{
			// lock of servers by server name (with wildcard)
			if (!InspIRCd::Match(ServerInstance->Config->ServerName, parameters[0]))
			{
				// Doesn't match us.
				return CMD_SUCCESS;
			}
		}
		locked = false;
		user->WriteNumeric(989, "%s %s :Open for new connections", user->nick.c_str(), user->server.c_str());
		ServerInstance->SNO->WriteGlobalSno('a', "Oper %s used UNLOCKSERV to allow for new connections", user->nick.c_str());
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		if (parameters.size() > 0 && parameters[0].find('*') != std::string::npos)
			return ROUTE_OPT_BCAST;
		if (parameters.size() > 0 && parameters[0].find('.') != std::string::npos)
			return ROUTE_OPT_UCAST(parameters[0]);
		return ROUTE_LOCALONLY;
	}
};

class ModuleLockserv : public Module
{
private:
	bool locked;
	CommandLockserv lockcommand;
	CommandUnlockserv unlockcommand;

public:
	ModuleLockserv() : lockcommand(this, locked), unlockcommand(this, locked) {}

	void init()
	{
		locked = false;
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
		// Emergency way to unlock
		if (!user) locked = false;
	}

	void OnUserRegister(LocalUser* user)
	{
		if (locked)
			ServerInstance->Users->QuitUser(user, "Server is temporarily closed. Please try again later.");
	}

	virtual ModResult OnCheckReady(LocalUser* user)
	{
		return locked ? MOD_RES_DENY : MOD_RES_PASSTHRU;
	}

	virtual Version GetVersion()
	{
		return Version("Allows locking of the server to stop all incoming connections until unlocked again", VF_OPTCOMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleLockserv)
