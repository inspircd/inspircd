#include <stdio.h>

#include "users.h"
#include "channels.h"
#include "modules.h"
#include <string>


/* $ModDesc: Povides support for services +r user/chan modes and more */

class ModuleServices : public Module
{
	Server *Srv; 
 public:
	ModuleServices()
	{
		Srv = new Server;

		Srv->AddExtendedMode('r',MT_CHANNEL,false,0,0);
		Srv->AddExtendedMode('r',MT_CLIENT,false,0,0);
		Srv->AddExtendedMode('R',MT_CHANNEL,false,0,0);
		Srv->AddExtendedMode('R',MT_CLIENT,false,0,0);
		Srv->AddExtendedMode('M',MT_CHANNEL,false,0,0);
	}
	
	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		
		if (modechar == 'r')
  		{
			if (type == MT_CHANNEL)
			{
				// only a u-lined server may add or remove the +r mode.
				if ((Srv->IsUlined(user->nick)) || (Srv->IsUlined(user->server)))
				{
					return 1;
				}
				else
				{
					Srv->SendServ(user->fd,"500 "+std::string(user->nick)+" :Only a U-Lined server may modify the +r channel mode");
				}
			}
			else
			{
				if (!strcmp(user->server,""))
				{
					return 1;
				}
				else
				{
					Srv->SendServ(user->fd,"500 "+std::string(user->nick)+" :Only a server may modify the +r user mode");
				}
			}
		}
		else if (modechar == 'R')
		{
			if (type == MT_CHANNEL)
			{
				return 1;
			}
		}
		else if (modechar == 'M')
		{
			if (type == MT_CHANNEL)
			{
				return 1;
			}
		}

		return 0;
	}

	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string text)
	{
		if (target_type == TYPE_CHANNEL)
		{
			chanrec* c = (chanrec*)dest;
			if ((c->IsCustomModeSet('M')) && (!strchr(user->modes,'r')))
			{
				if ((Srv->IsUlined(user->nick)) || (Srv->IsUlined(user->server)))
				{
					// user is ulined, can speak regardless
					return 0;
				}
				// user messaging a +M channel and is not registered
				Srv->SendServ(user->fd,"477 "+std::string(user->nick)+" "+std::string(c->name)+" :You need a registered nickname to speak on this channel");
				return 1;
			}
		}
		if (target_type == TYPE_USER)
		{
			userrec* u = (userrec*)dest;
			if ((strchr(u->modes,'R')) && (!strchr(user->modes,'r')))
			{
				if ((Srv->IsUlined(user->nick)) || (Srv->IsUlined(user->server)))
				{
					// user is ulined, can speak regardless
					return 0;
				}
				// user messaging a +R user and is not registered
				Srv->SendServ(user->fd,"477 "+std::string(user->nick)+" "+std::string(u->nick)+" :You need a registered nickname to message this user");
				return 1;
			}
		}
		return 0;
	}
 	
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string text)
	{
		if (target_type == TYPE_CHANNEL)
		{
			chanrec* c = (chanrec*)dest;
			if ((c->IsCustomModeSet('M')) && (!strchr(user->modes,'r')))
			{
				if ((Srv->IsUlined(user->nick)) || (Srv->IsUlined(user->server)))
				{
					// user is ulined, can speak regardless
					return 0;
				}
				// user noticing a +M channel and is not registered
				Srv->SendServ(user->fd,"477 "+std::string(user->nick)+" "+std::string(c->name)+" :You need a registered nickname to speak on this channel");
				return 1;
			}
		}
		if (target_type == TYPE_USER)
		{
			userrec* u = (userrec*)dest;
			if ((strchr(u->modes,'R')) && (!strchr(user->modes,'r')))
			{
				if ((Srv->IsUlined(user->nick)) || (Srv->IsUlined(user->server)))
				{
					// user is ulined, can speak regardless
					return 0;
				}
				// user noticing a +R user and is not registered
				Srv->SendServ(user->fd,"477 "+std::string(user->nick)+" "+std::string(u->nick)+" :You need a registered nickname to message this user");
				return 1;
			}
		}
		return 0;
	}
 	
	virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname)
	{
		if (chan)
		{
			if (chan->IsCustomModeSet('R'))
			{
				if (!strchr(user->modes,'r'))
				{
					if ((Srv->IsUlined(user->nick)) || (Srv->IsUlined(user->server)))
					{
						// user is ulined, won't be stopped from joining
						return 0;
					}
					// joining a +R channel and not identified
					Srv->SendServ(user->fd,"477 "+std::string(user->nick)+" "+std::string(chan->name)+" :You need a registered nickname to join this channel");
					return 1;
				}
			}
		}
		return 0;
	}

	virtual ~ModuleServices()
	{
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0);
	}
	
	virtual void OnUserConnect(userrec* user)
	{
	}

};


class ModuleServicesFactory : public ModuleFactory
{
 public:
	ModuleServicesFactory()
	{
	}
	
	~ModuleServicesFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleServices;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleServicesFactory;
}

