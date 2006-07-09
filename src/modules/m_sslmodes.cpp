#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Provides support for unreal-style channel mode +z */

class SSLMode : public ModeHandler
{
 public:
	SSLMode() : ModeHandler('z', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('z'))
			{
				chanuserlist userlist = Srv->GetUsers(channel);
				for(unsigned int i = 0; i < userlist.size(); i++)
				{
					if(!userlist[i]->GetExt("ssl"))
					{
						WriteServ(source->fd, "490 %s %s :all members of the channel must be connected via SSL", source->nick, channel->name);
						return MODEACTION_DENY;
					}
				}
				return MODEACTION_ALLOW;
			}
			else
			{
				return MODEACTION_DENY;
			}
		}
		else
		{
			(channel->IsModeSet('z')) ? return MODEACTION_DENY : return MODEACTION_ALLOW;
		}
	}
};

class ModuleSSLModes : public Module
{
	Server *Srv;
	SSLMode* sslm;
	
 public:
	ModuleSSLModes(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;

		sslm = new SSLMode();
		Srv->AddMode(sslm, 'z');
	}

	void Implements(char* List)
	{
		List[I_On005Numeric] = List[I_OnUserPreJoin] = 1;
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

	virtual ~ModuleSSLModes()
	{
		DELETE(sslm);
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
