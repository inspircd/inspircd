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

/* $ModDesc: Allows for hashed oper passwords */

#include "inspircd.h"
#include "m_hash.h"

typedef std::map<irc::string, Module*> hashymodules;

/* Handle /MKPASSWD
 */
class CommandMkpasswd : public Command
{
	hashymodules &hashers;
	std::deque<std::string> &names;
 public:
	CommandMkpasswd(Module* Creator, hashymodules &h, std::deque<std::string> &n) : Command(Creator, "MKPASSWD", 2), hashers(h), names(n)
	{
		syntax = "<hashtype> <any-text>";
		Penalty = 5;
	}

	void MakeHash(User* user, const char* algo, const char* stuff)
	{
		/* Lets see if they gave us an algorithm which has been implemented */
		hashymodules::iterator x = hashers.find(algo);
		if (x != hashers.end())
		{
			HashRequest hash(creator, x->second, stuff);
			/* Now attempt to generate a hash */
			user->WriteServ("NOTICE %s :%s hashed password for %s is %s",
				user->nick.c_str(), algo, stuff, hash.hex().c_str());
		}
		else if (names.empty())
		{
			/* same idea as bug #569 */
			user->WriteServ("NOTICE %s :No hash provider modules are loaded", user->nick.c_str());
		}
		else
		{
			/* I dont do flying, bob. */
			user->WriteServ("NOTICE %s :Unknown hash type, valid hash types are: %s", user->nick.c_str(), irc::stringjoiner(", ", names, 0, names.size() - 1).GetJoined().c_str() );
		}
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		MakeHash(user, parameters[0].c_str(), parameters[1].c_str());

		return CMD_SUCCESS;
	}
};

class ModuleOperHash : public Module
{

	CommandMkpasswd cmd;
	hashymodules hashers; /* List of modules which implement HashRequest */
	std::deque<std::string> names; /* Module names which implement HashRequest */

	bool diduseiface; /* If we've called UseInterface yet. */
 public:

	ModuleOperHash()
		: cmd(this, hashers, names)
	{
		diduseiface = false;

		/* Read the config file first */
//		Conf = NULL;
		OnRehash(NULL);

		/* Find all modules which implement the interface 'HashRequest' */
		modulelist* ml = ServerInstance->Modules->FindInterface("HashRequest");

		/* Did we find any modules? */
		if (ml)
		{
			/* Yes, enumerate them all to find out the hashing algorithm name */
			for (modulelist::iterator m = ml->begin(); m != ml->end(); m++)
			{
				/* Make a request to it for its name, its implementing
				 * HashRequest so we know its safe to do this
				 */
				std::string name = HashNameRequest(this, *m).response;
				/* Build a map of them */
				hashers[name.c_str()] = *m;
				names.push_back(name);
			}
			/* UseInterface doesn't do anything if there are no providers, so we'll have to call it later if a module gets loaded later on. */
			ServerInstance->Modules->UseInterface("HashRequest");
			diduseiface = true;
		}

		ServerInstance->AddCommand(&cmd);
		Implementation eventlist[] = { I_OnPassCompare, I_OnLoadModule };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual ~ModuleOperHash()
	{
		if (diduseiface) ServerInstance->Modules->DoneWithInterface("HashRequest");
	}


	virtual void OnLoadModule(Module* mod, const std::string& name)
	{
		if (ServerInstance->Modules->ModuleHasInterface(mod, "HashRequest"))
		{
			ServerInstance->Logs->Log("m_password-hash",DEBUG, "Post-load registering hasher: %s", name.c_str());
			std::string sname = HashNameRequest(this, mod).response;
			hashers[sname.c_str()] = mod;
			names.push_back(sname);
			if (!diduseiface)
			{
				ServerInstance->Modules->UseInterface("HashRequest");
				diduseiface = true;
			}
		}
	}

	virtual ModResult OnPassCompare(Extensible* ex, const std::string &data, const std::string &input, const std::string &hashtype)
	{
		/* First, lets see what hash theyre using on this oper */
		hashymodules::iterator x = hashers.find(hashtype.c_str());

		/* Is this a valid hash name? (case insensitive) */
		if (x != hashers.end())
		{
			/* Compare the hash in the config to the generated hash */
			if (!strcasecmp(data.c_str(), HashRequest(this, x->second, input).hex().c_str()))
				return MOD_RES_ALLOW;
			/* No match, and must be hashed, forbid */
			else
				return MOD_RES_DENY;
		}

		/* Not a hash, fall through to strcmp in core */
		return MOD_RES_PASSTHRU;
	}

	virtual Version GetVersion()
	{
		return Version("Allows for hashed oper passwords",VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(ModuleOperHash)
