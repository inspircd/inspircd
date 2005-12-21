/*   +------------------------------------+
 *   | Inspire Internet Relay Chat Daemon |
 *   +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
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
#include "helperfuncs.h"
#include <vector>

/* $ModDesc: Provides aliases of commands. */

class Alias
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
		Server *Srv;
		ConfigReader *MyConf;
		std::vector<Alias> Aliases;

		/* XXX - small issue, why is this marked public when it's not (really) intended for external use
		 * Fixed 30/11/05 by Brain as suggestion by w00t */
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
	
		ModuleAlias(Server* Me)
			: Module::Module(Me)
		{
			Srv = Me;
			MyConf = new ConfigReader;
			ReadAliases();
		}
	
		virtual ~ModuleAlias()
		{
			delete MyConf;
		}
	
		virtual Version GetVersion()
		{
			return Version(1,0,0,1,VF_VENDOR);
		}

		virtual int OnPreCommand(std::string command, char **parameters, int pcnt, userrec *user)
		{
			userrec *u = NULL;
			irc::string c = command.c_str();
			
			for (unsigned int i = 0; i < Aliases.size(); i++)
			{
				log(DEBUG,"Check against alias %s: %s",Aliases[i].text.c_str(),c.c_str());
				if (Aliases[i].text == c)
				{
					if (Aliases[i].requires != "")
					{
						u = Srv->FindNick(Aliases[i].requires);
					}
				
					if ((Aliases[i].requires != "") && (!u))
					{
						Srv->SendServ(user->fd,"401 "+std::string(user->nick)+" "+Aliases[i].requires+" :is currently unavailable. Please try again later.");
						return 1;
					}
					if (Aliases[i].uline)
					{
						if (!Srv->IsUlined(u->server))
						{
							Srv->SendOpers("*** NOTICE -- Service "+Aliases[i].requires+" required by alias "+std::string(Aliases[i].text.c_str())+" is not on a u-lined server, possibly underhanded antics detected!"); 
							Srv->SendServ(user->fd,"401 "+std::string(user->nick)+" "+Aliases[i].requires+" :is an imposter! Please inform an IRC operator as soon as possible.");
							return 1;
						}
					}

					std::stringstream stuff(Aliases[i].replace_with);
					for (int j = 1; j < pcnt; j++)
					{
						if (j)
							stuff << " ";
						stuff << parameters[j];
					}

					std::vector<std::string> items;
					while (!stuff.eof())
					{
						std::string data;
						stuff >> data;
						items.push_back(data);
					}

					char* para[127];

					for (unsigned int j = 1; j < items.size(); j++)
						para[j-1] = (char*)items[j].c_str();

					Srv->CallCommandHandler(items[0],para,items.size()-1,user);
					return 1;
				}
			}
			return 0;
	 	}
  
		virtual void OnRehash(std::string parameter)
		{
			delete MyConf;
			MyConf = new ConfigReader;
		
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
	
		virtual Module * CreateModule(Server* Me)
		{
			return new ModuleAlias(Me);
		}
};


extern "C" void * init_module( void )
{
	return new ModuleAliasFactory;
}

