#include <stdio.h>

#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides support for unreal-style channel mode +c */

class ModuleBlockColor : public Module
{
	Server *Srv;
 public:
 
	ModuleBlockColor()
	{
		Srv = new Server;
		Srv->AddExtendedMode('c',MT_CHANNEL,false,0,0);
	}
	
	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string text)
	{
		if (target_type == TYPE_CHANNEL)
		{
			chanrec* c = (chanrec*)dest;
			char ctext[MAXBUF];
			snprintf(ctext,MAXBUF,"%s",text.c_str());
			if (c->IsCustomModeSet('c'))
			{
				if ((strchr(ctext,'\2')) || (strchr(ctext,'\3')) || (strchr(ctext,31)))
				{
					WriteServ(user->fd,"404 %s %s :Can't send colors to channel (+c set)",user->nick, c->name);
					return 1;
				}
			}
		}
		return 0;
	}
	
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string text)
	{
		if (target_type == TYPE_CHANNEL)
		{
			chanrec* c = (chanrec*)dest;
			char ctext[MAXBUF];
			snprintf(ctext,MAXBUF,"%s",text.c_str());
			if (c->IsCustomModeSet('c'))
			{
				if ((strchr(ctext,'\2')) || (strchr(ctext,'\3')) || (strchr(ctext,31)))
				{
					WriteServ(user->fd,"404 %s %s :Can't send colors to channel (+c set)",user->nick, c->name);
					return 1;
				}
			}
		}
		return 0;
	}
	
	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		// check if this is our mode character...
		if ((modechar == 'c') && (type == MT_CHANNEL))
  		{
			return 1;
		}
		else
		{
			return 0;
		}
	}

	virtual ~ModuleBlockColor()
	{
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0);
	}
};


class ModuleBlockColorFactory : public ModuleFactory
{
 public:
	ModuleBlockColorFactory()
	{
	}
	
	~ModuleBlockColorFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleBlockColor;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleBlockColorFactory;
}

