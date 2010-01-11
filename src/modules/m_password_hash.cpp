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

/* $ModDesc: Allows for hashed oper passwords */

#include "inspircd.h"
#include "m_hash.h"

/* Handle /MKPASSWD
 */
class CommandMkpasswd : public Command
{
 public:
	CommandMkpasswd(Module* Creator) : Command(Creator, "MKPASSWD", 2)
	{
		syntax = "<hashtype> <any-text>";
		Penalty = 5;
	}

	void MakeHash(User* user, const std::string& algo, const std::string& stuff)
	{
		HashProvider* hp = ServerInstance->Modules->FindDataService<HashProvider>("hash/" + algo);
		if (hp)
		{
			/* Now attempt to generate a hash */
			user->WriteServ("NOTICE %s :%s hashed password for %s is %s",
				user->nick.c_str(), algo.c_str(), stuff.c_str(), hp->hexsum(stuff).c_str());
		}
		else
		{
			/* I dont do flying, bob. */
			user->WriteServ("NOTICE %s :Unknown hash type", user->nick.c_str());
		}
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		MakeHash(user, parameters[0], parameters[1]);

		return CMD_SUCCESS;
	}
};

class ModuleOperHash : public Module
{
	CommandMkpasswd cmd;
 public:

	ModuleOperHash() : cmd(this)
	{
		/* Read the config file first */
		OnRehash(NULL);

		ServerInstance->AddCommand(&cmd);
		Implementation eventlist[] = { I_OnPassCompare };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}

	virtual ModResult OnPassCompare(Extensible* ex, const std::string &data, const std::string &input, const std::string &hashtype)
	{
		HashProvider* hp = ServerInstance->Modules->FindDataService<HashProvider>("hash/" + hashtype);

		/* Is this a valid hash name? */
		if (hp)
		{
			/* Compare the hash in the config to the generated hash */
			if (data == hp->hexsum(input))
				return MOD_RES_ALLOW;
			else
				/* No match, and must be hashed, forbid */
				return MOD_RES_DENY;
		}

		/* Not a hash, fall through to strcmp in core */
		return MOD_RES_PASSTHRU;
	}

	virtual Version GetVersion()
	{
		return Version("Allows for hashed oper passwords",VF_VENDOR);
	}
};

MODULE_INIT(ModuleOperHash)
