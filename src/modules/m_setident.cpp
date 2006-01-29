#include "users.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Provides support for the SETIDENT command */

Server *Srv;

class cmd_setident : public command_t
{
 public:
	cmd_setident() : command_t("SETIDENT", 'o', 1)
	{
		this->source = "m_setident.so";
	}

	void Handle(char **parameters, int pcnt, userrec *user)
	{
		for(unsigned int x = 0; x < strlen(parameters[0]); x++)
		{
			if(((parameters[0][x] >= 'A') && (parameters[0][x] <= '}')) || strchr(".-0123456789", parameters[0][x]))
				continue;
			
			WriteServ(user->fd, "NOTICE %s :*** Invalid characters in ident", user->nick);
			return;
		}
		
		WriteOpers("%s used SETIDENT to change their ident from '%s' to '%s'", user->nick, user->ident, parameters[0]);
		strlcpy(user->ident, parameters[0], IDENTMAX+2);
	}
};


class ModuleSetIdent : public Module
{
	cmd_setident*	mycommand;
	
 public:
	ModuleSetIdent(Server* Me) : Module::Module(Me)
	{
		Srv = Me;
		mycommand = new cmd_setident();
		Srv->AddCommand(mycommand);
	}
	
	virtual ~ModuleSetIdent()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_VENDOR);
	}
	
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleSetIdentFactory : public ModuleFactory
{
 public:
	ModuleSetIdentFactory()
	{
	}
	
	~ModuleSetIdentFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleSetIdent(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSetIdentFactory;
}

