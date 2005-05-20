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

#include <stdio.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Provides channel modes +a and +q */

char fakevalue[] = "on";

class ModuleChanProtect : public Module
{
	Server *Srv;
	bool FirstInGetsFounder;
	ConfigReader *Conf;
	
 public:
 
	ModuleChanProtect()
	{
	
		// here we initialise our module. Use new to create new instances of the required
		// classes.
		
		Srv = new Server;
		Conf = new ConfigReader;
		
		// set up our modes. We're using listmodes and not normal extmodes here.
		// listmodes only need one parameter as everything else is assumed by the
		// nature of the mode thats being created.
		Srv->AddExtendedListMode('a');
		Srv->AddExtendedListMode('q');
		
		// read our config options (main config file)
		FirstInGetsFounder = Conf->ReadFlag("options","noservices",0);
	}
	
        virtual void On005Numeric(std::string &output)
        {
                std::stringstream line(output);
                std::string temp1, temp2;
                while (!line.eof())
                {
                        line >> temp1;
                        if (temp1.substr(0,10) == "CHANMODES=")
                        {
                                // append the chanmode to the end
                                temp1 = temp1.substr(10,temp1.length());
                                temp1 = "CHANMODES=qa" + temp1;
                        }
                        temp2 = temp2 + temp1 + " ";
                }
		if (temp2.length())
	                output = temp2.substr(0,temp2.length()-1);
        }

	virtual void OnUserKick(userrec* source, userrec* user, chanrec* chan, std::string reason)
	{
		// FIX: when someone gets kicked from a channel we must remove their Extensibles!
		user->Shrink("cm_founder_"+std::string(chan->name));
		user->Shrink("cm_protect_"+std::string(chan->name));
	}

	virtual void OnUserPart(userrec* user, chanrec* channel)
	{
		// FIX: when someone parts a channel we must remove their Extensibles!
		user->Shrink("cm_founder_"+std::string(channel->name));
		user->Shrink("cm_protect_"+std::string(channel->name));
	}

	virtual void OnRehash()
	{
		// on a rehash we delete our classes for good measure and create them again.
		delete Conf;
		Conf = new ConfigReader;
		// re-read our config options on a rehash
		FirstInGetsFounder = Conf->ReadFlag("options","noservices",0);
	}
	
	virtual void OnUserJoin(userrec* user, chanrec* channel)
	{
		// if the user is the first user into the channel, mark them as the founder, but only if
		// the config option for it is set
		if (FirstInGetsFounder)
		{
			if (Srv->CountUsers(channel) == 1)
			{
				// we're using Extensible::Extend to add data into user objects.
				// this way is best as it adds data thats accessible to other modules
				// (so long as you document your code properly) without breaking anything
				// because its encapsulated neatly in a map.
				if (user->Extend("cm_founder_"+std::string(channel->name),fakevalue))
				{
					Srv->Log(DEBUG,"Marked user "+std::string(user->nick)+" as founder for "+std::string(channel->name));
				}
			}
		}
	}
	
	virtual int OnAccessCheck(userrec* source,userrec* dest,chanrec* channel,int access_type)
	{
		// here we perform access checks, this is the important bit that actually stops kicking/deopping
		// etc of protected users. There are many types of access check, we're going to handle
		// a relatively small number of them relevent to our module using a switch statement.
	
		// don't allow action if:
		// (A) Theyre founder (no matter what)
		// (B) Theyre protected, and you're not
		// always allow the action if:
		// (A) The source is ulined
		
		
		// firstly, if a ulined nick, or a server, is setting the mode, then allow them to set the mode
		// without any access checks, we're not worthy :p
		if ((Srv->IsUlined(source->nick)) || (Srv->IsUlined(source->server)) || (!strcmp(source->server,"")))
		{
			return ACR_ALLOW;
		}

		switch (access_type)
		{
			// a user has been deopped. Do we let them? hmmm...
			case AC_DEOP:
				if (dest->GetExt("cm_founder_"+std::string(channel->name)))
				{
					Srv->SendServ(source->fd,"484 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't deop "+std::string(dest->nick)+" as the're a channel founder");
					return ACR_DENY;
				}
				if ((dest->GetExt("cm_protect_"+std::string(channel->name))) && (!source->GetExt("cm_protect_"+std::string(channel->name))))
				{
					Srv->SendServ(source->fd,"484 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't deop "+std::string(dest->nick)+" as the're protected (+a)");
					return ACR_DENY;
				}
			break;

			// a user is being kicked. do we chop off the end of the army boot?
			case AC_KICK:
				if (dest->GetExt("cm_founder_"+std::string(channel->name)))
				{
					Srv->SendServ(source->fd,"484 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't kick "+std::string(dest->nick)+" as the're a channel founder");
					return ACR_DENY;
				}
				if ((dest->GetExt("cm_protect_"+std::string(channel->name))) && (!source->GetExt("cm_protect_"+std::string(channel->name))))
				{
					Srv->SendServ(source->fd,"484 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't kick "+std::string(dest->nick)+" as the're protected (+a)");
					return ACR_DENY;
				}
			break;

			// a user is being dehalfopped. Yes, we do disallow -h of a +ha user
			case AC_DEHALFOP:
				if (dest->GetExt("cm_founder_"+std::string(channel->name)))
				{
					Srv->SendServ(source->fd,"484 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't de-halfop "+std::string(dest->nick)+" as the're a channel founder");
					return ACR_DENY;
				}
				if ((dest->GetExt("cm_protect_"+std::string(channel->name))) && (!source->GetExt("cm_protect_"+std::string(channel->name))))
				{
					Srv->SendServ(source->fd,"484 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't de-halfop "+std::string(dest->nick)+" as the're protected (+a)");
					return ACR_DENY;
				}
			break;

			// same with devoice.
			case AC_DEVOICE:
				if (dest->GetExt("cm_founder_"+std::string(channel->name)))
				{
					Srv->SendServ(source->fd,"484 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't devoice "+std::string(dest->nick)+" as the're a channel founder");
					return ACR_DENY;
				}
				if ((dest->GetExt("cm_protect_"+std::string(channel->name))) && (!source->GetExt("cm_protect_"+std::string(channel->name))))
				{
					Srv->SendServ(source->fd,"484 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't devoice "+std::string(dest->nick)+" as the're protected (+a)");
					return ACR_DENY;
				}
			break;
		}
		
		// we dont know what this access check is, or dont care. just carry on, nothing to see here.
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
			
			// source is a server, or ulined, we'll let them +-q the user.
			if ((Srv->IsUlined(user->nick)) || (Srv->IsUlined(user->server)) || (!strcmp(user->server,"")))
			{
				if (mode_on)
   				{
   					if (!theuser->GetExt("cm_founder_"+std::string(chan->name)))
   					{
						theuser->Extend("cm_founder_"+std::string(chan->name),fakevalue);
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
				// whoops, someones being naughty!
				WriteServ(user->fd,"468 %s %s :Only servers may set channel mode +q",user->nick, chan->name);
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

			// source has +q, is a server, or ulined, we'll let them +-a the user.
			if ((Srv->IsUlined(user->nick)) || (Srv->IsUlined(user->server)) || (!strcmp(user->server,"")) || (user->GetExt("cm_founder_"+std::string(chan->name))))
			{
				if (mode_on)
   				{
   					if (!theuser->GetExt("cm_protect_"+std::string(chan->name)))
   					{
						theuser->Extend("cm_protect_"+std::string(chan->name),fakevalue);
						return 1;
					}
				}
				else
    				{
    					if (theuser->GetExt("cm_protect_"+std::string(chan->name)))
    					{
						theuser->Shrink("cm_protect_"+std::string(chan->name));
						return 1;
					}
				}	

				return -1;
			}
			else
			{
				// bzzzt, wrong answer!
				WriteServ(user->fd,"482 %s %s :You are not a channel founder",user->nick, chan->name);
				return -1;
			}
		}
		return 0;
	}

	virtual void OnSendList(userrec* user, chanrec* channel, char mode)
	{
		if (mode == 'q')
		{
			chanuserlist cl = Srv->GetUsers(channel);
			for (int i = 0; i < cl.size(); i++)
			{
				if (cl[i]->GetExt("cm_founder_"+std::string(channel->name)))
				{
					WriteServ(user->fd,"386 %s %s %s",user->nick, channel->name,cl[i]->nick);
				}
			}
			WriteServ(user->fd,"387 %s %s :End of channel founder list",user->nick, channel->name);
		}
                if (mode == 'a')
                {
                        chanuserlist cl = Srv->GetUsers(channel);
                        for (int i = 0; i < cl.size(); i++)
                        {
                                if (cl[i]->GetExt("cm_protect_"+std::string(channel->name)))
                                {
                                        WriteServ(user->fd,"388 %s %s %s",user->nick, channel->name,cl[i]->nick);
                                }
                        }
			WriteServ(user->fd,"389 %s %s :End of channel protected user list",user->nick, channel->name);
                }

	}
	
	virtual ~ModuleChanProtect()
	{
		delete Conf;
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
	}
	
	virtual string_list OnChannelSync(chanrec* chan)
	{
		// this is called when the server is linking into a net and wants to sync channel data.
		// we should send our mode changes for the channel here to ensure that other servers
		// know whos +q/+a on the channel.
		chanuserlist cl = Srv->GetUsers(chan);
		string_list commands;
		for (int i = 0; i < cl.size(); i++)
		{
			if (cl[i]->GetExt("cm_founder_"+std::string(chan->name)))
			{
				commands.push_back("M "+std::string(chan->name)+" +q "+std::string(cl[i]->nick));
			}
			if (cl[i]->GetExt("cm_protect_"+std::string(chan->name)))
			{
				commands.push_back("M "+std::string(chan->name)+" +a "+std::string(cl[i]->nick));
			}
		}
		return commands;
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

