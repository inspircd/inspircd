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

#include <stdio.h>
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides support for unreal-style oper-override */

char dummyvalue[] = "on";

class ModuleOverride : public Module
{
	Server *Srv;
	bool NoisyOverride;
	ConfigReader *Conf;
	
 public:
 
	ModuleOverride()
	{
	
		// here we initialise our module. Use new to create new instances of the required
		// classes.
		
		Srv = new Server;
		Conf = new ConfigReader;
		
		// read our config options (main config file)
		NoisyOverride = Conf->ReadFlag("override","noisy",0);
	}
	
	virtual void OnRehash()
	{
		// on a rehash we delete our classes for good measure and create them again.
		delete Conf;
		Conf = new ConfigReader;
		// re-read our config options on a rehash
		NoisyOverride = Conf->ReadFlag("override","noisy",0);
	}

        virtual void On005Numeric(std::string &output)
        {
		output = output + std::string(" OVERRIDE");
        }
	
	virtual int OnAccessCheck(userrec* source,userrec* dest,chanrec* channel,int access_type)
	{
		if (strchr(source->modes,'o'))
		{
			if ((Srv) && (source) && (channel))
			{
				if ((Srv->ChanMode(source,channel) != "%") && (Srv->ChanMode(source,channel) != "@"))
				{
					switch (access_type)
					{
						case AC_KICK:
							Srv->SendOpers("*** NOTICE: "+std::string(source->nick)+" Override-Kicked "+std::string(dest->nick)+" on "+std::string(channel->name));
						break;
						case AC_DEOP:
							Srv->SendOpers("*** NOTICE: "+std::string(source->nick)+" Override-Deopped "+std::string(dest->nick)+" on "+std::string(channel->name));
						break;
						case AC_OP:
							Srv->SendOpers("*** NOTICE: "+std::string(source->nick)+" Override-Opped "+std::string(dest->nick)+" on "+std::string(channel->name));
						break;
						case AC_VOICE:
							Srv->SendOpers("*** NOTICE: "+std::string(source->nick)+" Override-Voiced "+std::string(dest->nick)+" on "+std::string(channel->name));
						break;
						case AC_DEVOICE:
							Srv->SendOpers("*** NOTICE: "+std::string(source->nick)+" Override-Devoiced "+std::string(dest->nick)+" on "+std::string(channel->name));
						break;
						case AC_HALFOP:
							Srv->SendOpers("*** NOTICE: "+std::string(source->nick)+" Override-Halfopped "+std::string(dest->nick)+" on "+std::string(channel->name));
						break;
						case AC_DEHALFOP:
							Srv->SendOpers("*** NOTICE: "+std::string(source->nick)+" Override-Dehalfopped "+std::string(dest->nick)+" on "+std::string(channel->name));
						break;
					}
				}
			}
			return ACR_ALLOW;
		}

		return ACR_DEFAULT;
	}
	
	virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname)
	{
		if (strchr(user->modes,'o'))
		{
			if (chan)
			{
				if ((chan->inviteonly) || (chan->key[0]) || (chan->limit >= Srv->CountUsers(chan)))
				{
					if (NoisyOverride)
					{
						if (!user->IsInvited(chan->name))
						{
							WriteChannelWithServ((char*)Srv->GetServerName().c_str(),chan,user,"NOTICE %s :%s invited himself into the channel",cname,user->nick);
						}
					}
					Srv->SendOpers("*** "+std::string(user->nick)+" used operoverride to bypass +i, +k or +l on "+std::string(cname));
				}
				return -1;
			}
		}
		return 0;
	}
	
	virtual ~ModuleOverride()
	{
		delete Conf;
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0);
	}
};


class ModuleOverrideFactory : public ModuleFactory
{
 public:
	ModuleOverrideFactory()
	{
	}
	
	~ModuleOverrideFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleOverride;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleOverrideFactory;
}

