/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
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

#include "users.h"
#include "channels.h"
#include "modules.h"
#include "configreader.h"
#include "inspircd.h"
#include "wildcard.h"

/* $ModDesc: Provides support for unreal-style oper-override */

typedef std::map<std::string,std::string> override_t;

class ModuleOverride : public Module
{
	
	override_t overrides;
	bool NoisyOverride;
 public:
 
	ModuleOverride(InspIRCd* Me)
		: Module::Module(Me)
	{		
		// read our config options (main config file)
		OnRehash("");
		ServerInstance->SNO->EnableSnomask('O',"OVERRIDE");
	}
	
	virtual void OnRehash(const std::string &parameter)
	{
		// on a rehash we delete our classes for good measure and create them again.
		ConfigReader* Conf = new ConfigReader(ServerInstance);
		
		// re-read our config options on a rehash
		NoisyOverride = Conf->ReadFlag("override","noisy",0);
		overrides.clear();
		for (int j =0; j < Conf->Enumerate("type"); j++)
		{
			std::string typen = Conf->ReadValue("type","name",j);
			std::string tokenlist = Conf->ReadValue("type","override",j);
			overrides[typen] = tokenlist;
		}
		
		DELETE(Conf);
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = List[I_OnAccessCheck] = List[I_On005Numeric] = List[I_OnUserPreJoin] = List[I_OnUserPreKick] = 1;
	}

	virtual void On005Numeric(std::string &output)
	{
		output.append(" OVERRIDE");
	}

	virtual bool CanOverride(userrec* source, char* token)
	{
		// checks to see if the oper's type has <type:override>
		override_t::iterator j = overrides.find(source->oper);
		
		if (j != overrides.end())
		{
			// its defined, return its value as a boolean for if the token is set
			return (j->second.find(token, 0) != std::string::npos);
		}
		
		// its not defined at all, count as false
		return false;
	}

	virtual int OnUserPreKick(userrec* source, userrec* user, chanrec* chan, const std::string &reason)
	{
		if ((*source->oper) && (CanOverride(source,"KICK")))
		{
			if (((chan->GetStatus(source) == STATUS_HOP) && (chan->GetStatus(user) == STATUS_OP)) || (chan->GetStatus(source) < STATUS_VOICE))
			{
				ServerInstance->SNO->WriteToSnoMask('O',"NOTICE: "+std::string(source->nick)+" Override-Kicked "+std::string(user->nick)+" on "+std::string(chan->name)+" ("+reason+")");
			}
			/* Returning -1 explicitly allows the kick */
			return -1;
		}
		return 0;
	}
	
	virtual int OnAccessCheck(userrec* source,userrec* dest,chanrec* channel,int access_type)
	{
		if (*source->oper)
		{
			if (source && channel)
			{
				// Fix by brain - allow the change if they arent on channel - rely on boolean short-circuit
				// to not check the other items in the statement if they arent on the channel
				int mode = channel->GetStatus(source);
				switch (access_type)
				{
					case AC_DEOP:
						if (CanOverride(source,"MODEDEOP"))
						{
							if ((!channel->HasUser(source)) || (mode != STATUS_OP))
								ServerInstance->SNO->WriteToSnoMask('O',"NOTICE: "+std::string(source->nick)+" Override-Deopped "+std::string(dest->nick)+" on "+std::string(channel->name));
							return ACR_ALLOW;
						}
						else
						{
							return ACR_DEFAULT;
						}
					case AC_OP:
						if (CanOverride(source,"MODEOP"))
						{
							if ((!channel->HasUser(source)) || (mode != STATUS_OP))
								ServerInstance->SNO->WriteToSnoMask('O',"NOTICE: "+std::string(source->nick)+" Override-Opped "+std::string(dest->nick)+" on "+std::string(channel->name));
							return ACR_ALLOW;
						}
						else
						{
							return ACR_DEFAULT;
						}
					case AC_VOICE:
						if (CanOverride(source,"MODEVOICE"))
						{
							if ((!channel->HasUser(source)) || (mode < STATUS_HOP))
								ServerInstance->SNO->WriteToSnoMask('O',"NOTICE: "+std::string(source->nick)+" Override-Voiced "+std::string(dest->nick)+" on "+std::string(channel->name));
							return ACR_ALLOW;
						}
						else
						{
							return ACR_DEFAULT;
						}
					case AC_DEVOICE:
						if (CanOverride(source,"MODEDEVOICE"))
						{
							if ((!channel->HasUser(source)) || (mode < STATUS_HOP))
								ServerInstance->SNO->WriteToSnoMask('O',"NOTICE: "+std::string(source->nick)+" Override-Devoiced "+std::string(dest->nick)+" on "+std::string(channel->name));
							return ACR_ALLOW;
						}
						else
						{
							return ACR_DEFAULT;
						}
					case AC_HALFOP:
						if (CanOverride(source,"MODEHALFOP"))
						{
							if ((!channel->HasUser(source)) || (mode != STATUS_OP))
								ServerInstance->SNO->WriteToSnoMask('O',"NOTICE: "+std::string(source->nick)+" Override-Halfopped "+std::string(dest->nick)+" on "+std::string(channel->name));
							return ACR_ALLOW;
						}
						else
						{
							return ACR_DEFAULT;
						}
					case AC_DEHALFOP:
						if (CanOverride(source,"MODEDEHALFOP"))
						{
							if ((!channel->HasUser(source)) || (mode != STATUS_OP))
								ServerInstance->SNO->WriteToSnoMask('O',"NOTICE: "+std::string(source->nick)+" Override-Dehalfopped "+std::string(dest->nick)+" on "+std::string(channel->name));
							return ACR_ALLOW;
						}
						else
						{
							return ACR_DEFAULT;
					}
				}
			
				if (CanOverride(source,"OTHERMODE"))
				{
					if ((!channel->HasUser(source)) || (mode != STATUS_OP))
						ServerInstance->SNO->WriteToSnoMask('O',"NOTICE: "+std::string(source->nick)+" changed modes on "+std::string(channel->name));
					return ACR_ALLOW;
				}
				else
				{
					return ACR_DEFAULT;
				}
			}
		}

		return ACR_DEFAULT;
	}
	
	virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname)
	{
		if (*user->oper)
		{
			if (chan)
			{
				if ((chan->modes[CM_INVITEONLY]) && (CanOverride(user,"INVITE")))
				{
					if (NoisyOverride)
					{
						irc::string x = chan->name;
						if (!user->IsInvited(x))
						{
							/* XXX - Ugly cast for a parameter that isn't used? :< - Om */
							chan->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :%s used oper-override to bypass invite-only", cname, user->nick);
						}
					}
					ServerInstance->SNO->WriteToSnoMask('O',std::string(user->nick)+" used operoverride to bypass +i on "+std::string(cname));
					return -1;
				}
				
				if ((chan->key[0]) && (CanOverride(user,"KEY")))
				{
					if (NoisyOverride)
						chan->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :%s used oper-override to bypass the channel key", cname, user->nick);
					ServerInstance->SNO->WriteToSnoMask('O',std::string(user->nick)+" used operoverride to bypass +k on "+std::string(cname));
					return -1;
				}
					
				if ((chan->limit > 0) && (chan->GetUserCounter() >=  chan->limit) && (CanOverride(user,"LIMIT")))
				{
					if (NoisyOverride)
						chan->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :%s used oper-override to bypass the channel limit", cname, user->nick);
					ServerInstance->SNO->WriteToSnoMask('O',std::string(user->nick)+" used operoverride to bypass +l on "+std::string(cname));
					return -1;
				}

				if (CanOverride(user,"BANWALK"))
				{
					if (chan->IsBanned(user))
					{
						if (NoisyOverride)
						{
							chan->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :%s used oper-override to bypass channel ban", cname, user->nick);
							ServerInstance->SNO->WriteToSnoMask('O',"%s used oper-override to bypass channel ban", cname, user->nick);
						}
					}
					return -1;
				}
			}
		}
		return 0;
	}
	
	virtual ~ModuleOverride()
	{
		ServerInstance->SNO->DisableSnomask('O');
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleOverride(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleOverrideFactory;
}
