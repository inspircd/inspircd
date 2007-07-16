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

/* $ModDesc: Allows for hashed oper passwords */
/* $ModDep: m_hash.h */

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "m_hash.h"

typedef std::map<irc::string, Module*> hashymodules;

/* Handle /MKPASSWD
 */
class cmd_mkpasswd : public command_t
{
	Module* Sender;
	hashymodules &hashers;
	std::deque<std::string> &names;
 public:
	cmd_mkpasswd (InspIRCd* Instance, Module* S, hashymodules &h, std::deque<std::string> &n)
		: command_t(Instance,"MKPASSWD", 'o', 2), Sender(S), hashers(h), names(n)
	{
		this->source = "m_oper_hash.so";
		syntax = "<hashtype> <any-text>";
	}

	void MakeHash(userrec* user, const char* algo, const char* stuff)
	{
		/* Lets see if they gave us an algorithm which has been implemented */
		hashymodules::iterator x = hashers.find(algo);
		if (x != hashers.end())
		{
			/* Yup, reset it first (Always ALWAYS do this) */
			HashResetRequest(Sender, x->second).Send();
			/* Now attempt to generate a hash */
			user->WriteServ("NOTICE %s :%s hashed password for %s is %s",user->nick, algo, stuff, HashSumRequest(Sender, x->second, stuff).Send() );
		}
		else
		{
			/* I dont do flying, bob. */
			user->WriteServ("NOTICE %s :Unknown hash type, valid hash types are: %s", user->nick, irc::stringjoiner(", ", names, 0, names.size() - 1).GetJoined().c_str() );
		}
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		MakeHash(user, parameters[0], parameters[1]);
		/* NOTE: Don't propogate this across the network!
		 * We dont want plaintext passes going all over the place...
		 * To make sure it goes nowhere, return CMD_FAILURE!
		 */
		return CMD_FAILURE;
	}
};

class ModuleOperHash : public Module
{
	
	cmd_mkpasswd* mycommand;
	ConfigReader* Conf;
	hashymodules hashers; /* List of modules which implement HashRequest */
	std::deque<std::string> names; /* Module names which implement HashRequest */

 public:

	ModuleOperHash(InspIRCd* Me)
		: Module(Me)
	{

		/* Read the config file first */
		Conf = NULL;
		OnRehash(NULL,"");

		ServerInstance->UseInterface("HashRequest");

		/* Find all modules which implement the interface 'HashRequest' */
		modulelist* ml = ServerInstance->FindInterface("HashRequest");

		/* Did we find any modules? */
		if (ml)
		{
			/* Yes, enumerate them all to find out the hashing algorithm name */
			for (modulelist::iterator m = ml->begin(); m != ml->end(); m++)
			{
				/* Make a request to it for its name, its implementing
				 * HashRequest so we know its safe to do this
				 */
				std::string name = HashNameRequest(this, *m).Send();
				/* Build a map of them */
				hashers[name.c_str()] = *m;
				names.push_back(name);
			}
		}
		else
		{
			throw ModuleException("I can't find any modules loaded which implement the HashRequest interface! You probably forgot to load a hashing module such as m_md5.so or m_sha256.so.");
		}

		mycommand = new cmd_mkpasswd(ServerInstance, this, hashers, names);
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~ModuleOperHash()
	{
		ServerInstance->DoneWithInterface("HashRequest");
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = List[I_OnOperCompare] = 1;
	}

	virtual void OnRehash(userrec* user, const std::string &parameter)
	{
		/* Re-read configuration file */
		if (Conf)
			delete Conf;

		Conf = new ConfigReader(ServerInstance);
	}

	virtual int OnOperCompare(const std::string &data, const std::string &input, int tagnumber)
	{
		/* First, lets see what hash theyre using on this oper */
		std::string hashtype = Conf->ReadValue("oper", "hash", tagnumber);
		hashymodules::iterator x = hashers.find(hashtype.c_str());

		/* Is this a valid hash name? (case insensitive) */
		if (x != hashers.end())
		{
			/* Reset the hashing module */
			HashResetRequest(this, x->second).Send();
			/* Compare the hash in the config to the generated hash */
			if (!strcasecmp(data.c_str(), HashSumRequest(this, x->second, input.c_str()).Send()))
				return 1;
			/* No match, and must be hashed, forbid */
			else return -1;
		}

		/* Not a hash, fall through to strcmp in core */
		return 0;
	}

	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(ModuleOperHash)
