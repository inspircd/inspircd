/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $ModDesc: Allows for hashed oper passwords */
/* $ModDep: m_hash.h */

using namespace std;

#include "inspircd_config.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"

#include "m_hash.h"

/* Handle /MKPASSWD
 */
class cmd_mkpasswd : public command_t
{
	Module* Sender;
	std::map<irc::string, Module*> &hashers;
	std::deque<std::string> &names;
 public:
	cmd_mkpasswd (InspIRCd* Instance, Module* S, std::map<irc::string, Module*> &h, std::deque<std::string> &n)
		: command_t(Instance,"MKPASSWD", 'o', 2), Sender(S), hashers(h), names(n)
	{
		this->source = "m_oper_hash.so";
		syntax = "<hashtype> <any-text>";
	}

	void MakeHash(userrec* user, const char* algo, const char* stuff)
	{
		std::map<irc::string, Module*>::iterator x = hashers.find(algo);
		if (x != hashers.end())
		{
			HashResetRequest(Sender, x->second).Send();
			user->WriteServ("NOTICE %s :%s hashed password for %s is %s",user->nick, algo, stuff, HashSumRequest(Sender, x->second, stuff).Send() );
		}
		else
		{
			user->WriteServ("NOTICE %s :Unknown hash type, valid hash types are: %s", user->nick, irc::stringjoiner(",", names, 0, names.size() - 1).GetJoined().c_str() );
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
	std::map<irc::string, Module*> hashers;
	std::deque<std::string> names;
	modulelist* ml;

 public:

	ModuleOperHash(InspIRCd* Me)
		: Module::Module(Me)
	{
		Conf = NULL;
		OnRehash("");

		modulelist* ml = ServerInstance->FindInterface("HashRequest");

		if (ml)
		{
			ServerInstance->Log(DEBUG, "Found interface 'HashRequest' containing %d modules", ml->size());

			for (modulelist::iterator m = ml->begin(); m != ml->end(); m++)
			{
				std::string name = HashNameRequest(this, *m).Send();
				hashers[name.c_str()] = *m;
				names.push_back(name);
				ServerInstance->Log(DEBUG, "Found HashRequest interface: '%s' -> '%08x'", name.c_str(), *m);
			}
		}

		mycommand = new cmd_mkpasswd(ServerInstance, this, hashers, names);
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~ModuleOperHash()
	{
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = List[I_OnOperCompare] = 1;
	}

	virtual void OnRehash(const std::string &parameter)
	{
		if (Conf)
			delete Conf;

		Conf = new ConfigReader(ServerInstance);
	}

	virtual int OnOperCompare(const std::string &data, const std::string &input, int tagnumber)
	{
		std::string hashtype = Conf->ReadValue("oper", "hash", tagnumber);
		std::map<irc::string, Module*>::iterator x = hashers.find(hashtype.c_str());

		if (x != hashers.end())
		{
			HashResetRequest(this, x->second).Send();
			if (!strcasecmp(data.c_str(), HashSumRequest(this, x->second, input.c_str()).Send()))
				return 1;
			else return -1;
		}

		return 0;
	}

	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
};


class ModuleOperHashFactory : public ModuleFactory
{
 public:
	ModuleOperHashFactory()
	{
	}
	
	~ModuleOperHashFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleOperHash(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleOperHashFactory;
}
