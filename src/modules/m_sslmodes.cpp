#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Provides support for unreal-style channel mode +z */

class ModuleSSLModes : public Module
{
	Server *Srv;
	
 public:
	ModuleSSLModes(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		Srv->AddExtendedMode('z', MT_CHANNEL, false, 0, 0);
	}

	void Implements(char* List)
	{
		List[I_OnExtendedMode] = List[I_On005Numeric] = List[I_OnUserPreJoin] = 1;
	}

	virtual void On005Numeric(std::string &output)
	{
		InsertMode(output, "z", 4);
	}
	
	virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname)
	{
		if(chan && chan->IsModeSet('z'))
		{
			if(user->GetExt("ssl"))
			{
				// Let them in
				return 0;
			}
			else
			{
				// Deny
				WriteServ(user->fd, "489 %s %s :Cannot join channel (+z)", user->nick, cname);
				return 1;
			}
		}
		
		return 0;
	}
	
	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		// check if this is our mode character...
		if ((modechar == 'z') && (type == MT_CHANNEL))
  		{
  			chanrec* chan = (chanrec*)target;
  			
  			chanuserlist userlist = Srv->GetUsers(chan);
  			
  			for(unsigned int i = 0; i < userlist.size(); i++)
  			{
  				if(!userlist[i]->GetExt("ssl"))
  				{
  					WriteServ(user->fd, "490 %s %s :all members of the channel must be connected via SSL", user->nick, chan->name);
  					return 0;
  				}
  			}
  			
			return 1;
		}
		else
		{
			return 0;
		}
	}

	virtual ~ModuleSSLModes()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_STATIC | VF_VENDOR);
	}
};


class ModuleSSLModesFactory : public ModuleFactory
{
 public:
	ModuleSSLModesFactory()
	{
	}
	
	~ModuleSSLModesFactory()
	{
	}
	
	virtual Module* CreateModule(Server* Me)
	{
		return new ModuleSSLModes(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSSLModesFactory;
}
