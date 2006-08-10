#include <string>
#include "users.h"
#include "modules.h"
#include "helperfuncs.h"
#include "inspircd.h"

/* $ModDesc: Provides support for the CHGIDENT command */

extern InspIRCd* ServerInstance;

class cmd_chgident : public command_t
{
 public:
	cmd_chgident() : command_t("CHGIDENT", 'o', 2)
	{
		this->source = "m_chgident.so";
		syntax = "<nick> <newident>";
	}
	
	void Handle(const char** parameters, int pcnt, userrec *user)
	{
		userrec* dest = ServerInstance->FindNick(parameters[0]);

		if(dest)
		{
			if(!ServerInstance->IsIdent(parameters[1]))
			{
				user->WriteServ("NOTICE %s :*** Invalid characters in ident", user->nick);
				return;
			}
		
			ServerInstance->WriteOpers("%s used CHGIDENT to change %s's ident from '%s' to '%s'", user->nick, dest->nick, dest->ident, parameters[1]);
			strlcpy(dest->ident, parameters[1], IDENTMAX+2);
		}
		else
		{
			user->WriteServ("401 %s %s :No such nick/channel", user->nick, parameters[0]);
		}
	}
};


class ModuleChgIdent : public Module
{
	cmd_chgident* mycommand;
	Server* Srv;
	
public:
	ModuleChgIdent(InspIRCd* Me) : Module::Module(Me)
	{
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleChgIdent(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleChgIdentFactory;
}

