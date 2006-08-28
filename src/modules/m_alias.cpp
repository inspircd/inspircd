/*   +------------------------------------+
 *   | Inspire Internet Relay Chat Daemon |
 *   +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *   E-mail:
 *	<brain@chatspike.net>
 *	<Craig@chatspike.net>
 * 
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include "users.h"
#include "channels.h"
#include "modules.h"
#include "commands.h"
#include "inspircd.h"
#include <vector>

/* $ModDesc: Provides aliases of commands. */

class Alias : public classbase
{
 public:
	irc::string text;
	std::string replace_with;
	std::string requires;
	bool uline;
};

class ModuleAlias : public Module
{
 private:
	ConfigReader *MyConf;
	std::vector<Alias> Aliases;

	virtual void ReadAliases()
	{
		Aliases.clear();
	
		for (int i = 0; i < MyConf->Enumerate("alias"); i++)
		{
			Alias a;
			std::string txt;
			txt = MyConf->ReadValue("alias", "text", i);
			a.text = txt.c_str();
			a.replace_with = MyConf->ReadValue("alias", "replace", i);
			a.requires = MyConf->ReadValue("alias", "requires", i);
		
			a.uline =	((MyConf->ReadValue("alias", "uline", i) == "yes") ||
					(MyConf->ReadValue("alias", "uline", i) == "1") ||
						(MyConf->ReadValue("alias", "uline", i) == "true"));
					
			Aliases.push_back(a);
		}

	}

 public:
	
	ModuleAlias(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		MyConf = new ConfigReader(ServerInstance);
		ReadAliases();
	}

	void Implements(char* List)
	{
		List[I_OnPreCommand] = List[I_OnRehash] = 1;
	}

	virtual ~ModuleAlias()
	{
		DELETE(MyConf);
	}

	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}

	virtual int OnPreCommand(const std::string &command, const char** parameters, int pcnt, userrec *user, bool validated)
	{
		userrec *u = NULL;
		irc::string c = command.c_str();
		/* If the command is valid, we dont want to know,
		 * and if theyre not registered yet, we dont want
		 * to know either
		 */
		if ((validated) || (user->registered != REG_ALL))
			return 0;
		
		for (unsigned int i = 0; i < Aliases.size(); i++)
		{
			if (Aliases[i].text == c)
			{
				if (Aliases[i].requires != "")
				{
					u = ServerInstance->FindNick(Aliases[i].requires);
					if (!u)
					{
						user->WriteServ("401 "+std::string(user->nick)+" "+Aliases[i].requires+" :is currently unavailable. Please try again later.");
						return 1;
					}
				}
				if ((u != NULL) && (Aliases[i].requires != "") && (Aliases[i].uline))
				{
					if (!ServerInstance->ULine(u->server))
					{
						ServerInstance->WriteOpers("*** NOTICE -- Service "+Aliases[i].requires+" required by alias "+std::string(Aliases[i].text.c_str())+" is not on a u-lined server, possibly underhanded antics detected!"); 
						user->WriteServ("401 "+std::string(user->nick)+" "+Aliases[i].requires+" :is an imposter! Please inform an IRC operator as soon as possible.");
						return 1;
					}
				}
					std::string n = "";
				for (int j = 0; j < pcnt; j++)
				{
					if (j)
						n = n + " ";
					n = n + parameters[j];
				}
				/* Final param now in n as one string */
				std::stringstream stuff(Aliases[i].replace_with);
					std::string cmd = "";
				std::string target = "";
				stuff >> cmd;
				stuff >> target;
					const char* para[2];
				para[0] = target.c_str();
				para[1] = n.c_str();
					ServerInstance->CallCommandHandler(cmd,para,2,user);
				return 1;
			}
		}
		return 0;
 	}
 
	virtual void OnRehash(const std::string &parameter)
	{
		DELETE(MyConf);
		MyConf = new ConfigReader(ServerInstance);
	
		ReadAliases();
 	}
};


class ModuleAliasFactory : public ModuleFactory
{
 public:
	ModuleAliasFactory()
	{
	}

	~ModuleAliasFactory()
	{
	}

		virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleAlias(Me);
	}
};


extern "C" void * init_module( void )
{
	return new ModuleAliasFactory;
}

