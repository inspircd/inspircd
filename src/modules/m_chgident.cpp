#include <string>
#include "users.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Provides support for the CHGIDENT command */

Server *Srv;


class cmd_chgident : public command_t
{
 public:
	cmd_chgident() : command_t("CHGIDENT", 'o', 2)
	{
		this->source = "m_chgident.so";
	}
	
	void Handle(char **parameters, int pcnt, userrec *user)
	{
		userrec* dest = Srv->FindNick(std::string(parameters[0]));
		
		if(dest)
		{
			for(unsigned int x = 0; x < strlen(parameters[1]); x++)
			{
				if(((parameters[1][x] >= 'A') && (parameters[1][x] <= '}')) || strchr(".-0123456789", parameters[1][x]))
					continue;
			
				WriteServ(user->fd, "NOTICE %s :*** Invalid characters in ident", user->nick);
				return;
			}
		
			WriteOpers("%s used CHGIDENT to change %s's host from '%s' to '%s'", user->nick, dest->nick, dest->ident, parameters[1]);
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
		Srv = Me;
		mycommand = new cmd_chgident();
		Srv->AddCommand(mycommand);
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

