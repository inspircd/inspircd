/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
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

using namespace std;

#include "users.h"
#include "channels.h"
#include "modules.h"
#include <vector>

/* $ModDesc: Changes the ident of connecting bottler clients to 'bottler' */

class Alias
{
 public:
 	std::string text;
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
 public:
 
	virtual void ReadAliases()
	{
		Aliases.clear();
		
		for (int i = 0; i < MyConf->Enumerate("alias"); i++)
		{
			Alias a;
			a.text = MyConf->ReadValue("alias", "text", i);
			a.replace_with = MyConf->ReadValue("alias", "replace", i);
			a.requires = MyConf->ReadValue("alias", "requires", i);
			
			a.uline =	((MyConf->ReadValue("alias", "uline", i) == "yes") ||
					(MyConf->ReadValue("alias", "uline", i) == "1") ||
     					(MyConf->ReadValue("alias", "uline", i) == "true"));
     					
			Aliases.push_back(a);
		}
	
	}
	
	ModuleAlias()
	{
		Srv = new Server;
		MyConf = new ConfigReader;

		ReadAliases();
	}
	
	virtual ~ModuleAlias()
	{
		delete Srv;
		delete MyConf;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}


	virtual void OnServerRaw(std::string &raw, bool inbound, userrec* user)
	{
		if (inbound)
		{
			char data[MAXBUF];
			strncpy(data,raw.c_str(),MAXBUF);
			char* dptr = data;
			
			for (unsigned int i = 0; i < Aliases.size(); i++)
			{
				if (!strncasecmp(Aliases[i].text.c_str(),data,Aliases[i].text.length()))
				{
					userrec* u = NULL;
					
					if (Aliases[i].requires != "")
					{
						u = Srv->FindNick(Aliases[i].requires);
					}
					
					if ((Aliases[i].requires != "") && (!u))
					{
						Srv->SendServ(user->fd,"401 "+std::string(user->nick)+" "+Aliases[i].requires+" :is currently unavailable. Please try again later.");
						raw = "PONG :"+Srv->GetServerName();
						return;
					}
					if (Aliases[i].uline)
					{
						if (!Srv->IsUlined(u->server))
						{
							Srv->SendOpers("*** NOTICE -- Service "+Aliases[i].requires+" required by alias "+Aliases[i].text+" is not on a u-lined server, possibly underhanded antics detected!"); 
							Srv->SendServ(user->fd,"401 "+std::string(user->nick)+" "+Aliases[i].requires+" :is an imposter! Please inform an IRC operator as soon as possible.");
							raw = "PONG :"+Srv->GetServerName();
							return;
						}
					}
					
					dptr += Aliases[i].text.length();
					if (strlen(dptr))
					{
						raw = Aliases[i].replace_with + std::string(dptr);
					}
					else
					{
						raw = Aliases[i].replace_with;
					}
					return;
				}
			}
		}
 	}
  
	virtual void OnRehash()
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
	
	virtual Module * CreateModule()
	{
		return new ModuleAlias;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleAliasFactory;
}

