#include <stdio.h>

#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides channel modes +a and +q */

char dummyvalue[] = "on";

class ModuleChanProtect : public Module
{
	Server *Srv;
	
 public:
 
	ModuleChanProtect()
	{
		Srv = new Server;
		// set up our modes. We're using listmodes and not normal extmodes here.
		// listmodes only need one parameter as everything else is assumed by the
		// nature of the mode thats being created.
		Srv->AddExtendedListMode('a');
		Srv->AddExtendedListMode('q');
	}
	
	virtual void OnUserJoin(userrec* user, chanrec* channel)
	{
		// if the user is the first user into the channel, mark them as the founder
		if (Srv->CountUsers(channel) == 1)
		{
			if (user->Extend("cm_founder_"+std::string(channel->name),dummyvalue))
			{
				Srv->Log(DEBUG,"Marked user "+std::string(user->nick)+" as founder for "+std::string(channel->name));
			}
		}
	}
	
	virtual int OnAccessCheck(userrec* source,userrec* dest,chanrec* channel,int access_type)
	{
		// don't allow action if:
		// (A) Theyre founder (no matter what)
		// (B) Theyre protected, and you're not
		switch (access_type)
		{
			case AC_DEOP:
				if (dest->GetExt("cm_founder_"+std::string(channel->name)))
				{
					Srv->SendServ(source->fd,"482 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't deop "+std::string(dest->nick)+" as the're a channel founder");
					return ACR_DENY;
				}
				if ((dest->GetExt("cm_protect_"+std::string(channel->name))) && (!source->GetExt("cm_protect_"+std::string(channel->name))))
				{
					Srv->SendServ(source->fd,"482 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't deop "+std::string(dest->nick)+" as the're protected (+a)");
					return ACR_DENY;
				}
			break;

			case AC_KICK:
				if (dest->GetExt("cm_founder_"+std::string(channel->name)))
				{
					Srv->SendServ(source->fd,"482 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't kick "+std::string(dest->nick)+" as the're a channel founder");
					return ACR_DENY;
				}
				if ((dest->GetExt("cm_protect_"+std::string(channel->name))) && (!source->GetExt("cm_protect_"+std::string(channel->name))))
				{
					Srv->SendServ(source->fd,"482 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't kick "+std::string(dest->nick)+" as the're protected (+a)");
					return ACR_DENY;
				}
			break;

			case AC_DEHALFOP:
				if (dest->GetExt("cm_founder_"+std::string(channel->name)))
				{
					Srv->SendServ(source->fd,"482 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't de-halfop "+std::string(dest->nick)+" as the're a channel founder");
					return ACR_DENY;
				}
				if ((dest->GetExt("cm_protect_"+std::string(channel->name))) && (!source->GetExt("cm_protect_"+std::string(channel->name))))
				{
					Srv->SendServ(source->fd,"482 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't de-halfop "+std::string(dest->nick)+" as the're protected (+a)");
					return ACR_DENY;
				}
			break;

			case AC_DEVOICE:
				if (dest->GetExt("cm_founder_"+std::string(channel->name)))
				{
					Srv->SendServ(source->fd,"482 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't devoice "+std::string(dest->nick)+" as the're a channel founder");
					return ACR_DENY;
				}
				if ((dest->GetExt("cm_protect_"+std::string(channel->name))) && (!source->GetExt("cm_protect_"+std::string(channel->name))))
				{
					Srv->SendServ(source->fd,"482 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't devoice "+std::string(dest->nick)+" as the're protected (+a)");
					return ACR_DENY;
				}
			break;
		}
		return ACR_DEFAULT;
	}
	
	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		// not out mode, bail
		if ((modechar == 'q') && (type == MT_CHANNEL))
		{
			// set up parameters
			chanrec* chan = (chanrec*)target;
			userrec* theuser = Srv->FindNick(params[0]);
		
			// cant find the user given as the parameter, eat the mode change.
			if (!theuser)
				return -1;
			
			// given user isnt even on the channel, eat the mode change
			if (!Srv->IsOnChannel(theuser,chan))
				return -1;
			
			if ((Srv->IsUlined(user->nick)) || (Srv->IsUlined(user->server)) || (!strcmp(user->server,"")))
			{
				if (mode_on)
   				{
   					if (!theuser->GetExt("cm_founder_"+std::string(chan->name)))
   					{
						theuser->Extend("cm_founder_"+std::string(chan->name),dummyvalue);
						return 1;
					}
				}
				else
 				{
 					if (theuser->GetExt("cm_founder_"+std::string(chan->name)))
 					{
						theuser->Shrink("cm_founder_"+std::string(chan->name));
						return 1;
					}
				}	

				return -1;
			}
			else
			{
				WriteServ(user->fd,"482 %s %s :Only servers may set channel mode +q",user->nick, chan->name);
				return -1;
			}
		}
		if ((modechar == 'a') && (type == MT_CHANNEL))
		{
			// set up parameters
			chanrec* chan = (chanrec*)target;
			userrec* theuser = Srv->FindNick(params[0]);
		
			// cant find the user given as the parameter, eat the mode change.
			if (!theuser)
				return -1;
			
			// given user isnt even on the channel, eat the mode change
			if (!Srv->IsOnChannel(theuser,chan))
				return -1;
			
			if ((Srv->IsUlined(user->nick)) || (Srv->IsUlined(user->server)) || (!strcmp(user->server,"")) || (user->GetExt("cm_founder_"+std::string(chan->name))))
			{
				if (mode_on)
   				{
   					if (!theuser->GetExt("cm_founder_"+std::string(chan->name)))
   					{
						theuser->Extend("cm_protect_"+std::string(chan->name),dummyvalue);
						return 1;
					}
				}
				else
    				{
    					if (theuser->GetExt("cm_founder_"+std::string(chan->name)))
    					{
						theuser->Shrink("cm_protect_"+std::string(chan->name));
						return 1;
					}
				}	

				return -1;
			}
			else
			{
				WriteServ(user->fd,"482 %s %s :You are not a channel founder",user->nick, chan->name);
				return -1;
			}
		}
		return 0;
	}
	
	virtual ~ModuleChanProtect()
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


class ModuleChanProtectFactory : public ModuleFactory
{
 public:
	ModuleChanProtectFactory()
	{
	}
	
	~ModuleChanProtectFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleChanProtect;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleChanProtectFactory;
}

