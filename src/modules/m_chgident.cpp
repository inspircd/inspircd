#include <string>
#include "users.h"
#include "modules.h"
#include "message.h"
#include "helperfuncs.h"

/* $ModDesc: Provides support for the CHGIDENT command */

class cmd_chgident : public command_t
{
	Server* Srv;
 public:
	cmd_chgident(Server* serv) : command_t("CHGIDENT", 'o', 2)
	{
		this->source = "m_chgident.so";
		Srv = serv;
	}
	
	void Handle(char **parameters, int pcnt, userrec *user)
	{
		userrec* dest = Srv->FindNick(std::string(parameters[0]));
		
		if(dest)
		{
			if(!isident(parameters[1]))
			{
				WriteServ(user->fd, "NOTICE %s :*** Invalid characters in ident", user->nick);
				return;
			}
		
			WriteOpers("%s used CHGIDENT to change %s's ident from '%s' to '%s'", user->nick, dest->nick, dest->ident, parameters[1]);
			strlcpy(dest->ident, parameters[1], IDENTMAX+2);
		}
		else
		{
			WriteServ(user->fd, "401 %s %s :No such nick/channel", user->nick, parameters[0]);
		}
	}
};


class ModuleChgIdent : public Module
{
	cmd_chgident* mycommand;
	
public:
	ModuleChgIdent(Server* Me) : Module::Module(Me)
	{
		mycommand = new cmd_chgident(Me);
		Me->AddCommand(mycommand);
	}
	
	virtual ~ModuleChgIdent()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_VENDOR);
	}
	
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleChgIdentFactory : public ModuleFactory
{
 public:
	ModuleChgIdentFactory()
	{
	}
	
	~ModuleChgIdentFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleChgIdent(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleChgIdentFactory;
}

