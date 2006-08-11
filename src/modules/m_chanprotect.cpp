/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *		       E-mail:
 *		<brain@chatspike.net>
 *	   	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"
#include "inspircd.h"
#include "commands.h"

/* $ModDesc: Provides channel modes +a and +q */



const char* fakevalue = "on";

class ChanFounder : public ModeHandler
{
	char* dummyptr;
 public:
	ChanFounder(InspIRCd* Instance) : ModeHandler(Instance, 'q', 1, 1, true, MODETYPE_CHANNEL, false) { }

	ModePair ModeSet(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter)
	{
		userrec* x = ServerInstance->FindNick(parameter);
		if (x)
		{
			if (!channel->HasUser(x))
			{
				return std::make_pair(false, parameter);
			}
			else
			{
				std::string founder = "cm_founder_"+std::string(channel->name);
				if (x->GetExt(founder,dummyptr))
				{
					return std::make_pair(true, x->nick);
				}
				else
				{
					return std::make_pair(false, parameter);
				}
			}
		}
		return std::make_pair(false, parameter);
	}


	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		userrec* theuser = ServerInstance->FindNick(parameter);

		log(DEBUG,"ChanFounder::OnModeChange");

		// cant find the user given as the parameter, eat the mode change.
		if (!theuser)
		{
			log(DEBUG,"No such user in ChanFounder");
			parameter = "";
			return MODEACTION_DENY;
		}

		// given user isnt even on the channel, eat the mode change
		if (!channel->HasUser(theuser))
		{
			log(DEBUG,"Channel doesn't have user in ChanFounder");
			parameter = "";
			return MODEACTION_DENY;
		}

		std::string protect = "cm_protect_"+std::string(channel->name);
		std::string founder = "cm_founder_"+std::string(channel->name);

		 // source is a server, or ulined, we'll let them +-q the user.
		if ((is_uline(source->nick)) || (is_uline(source->server)) || (!*source->server) || (!IS_LOCAL(source)))
		{
			log(DEBUG,"Allowing remote mode change in ChanFounder");
			if (adding)
			{
				if (!theuser->GetExt(founder,dummyptr))
				{
					log(DEBUG,"Does not have the ext item in ChanFounder");
					if (!theuser->Extend(founder,fakevalue))
						log(DEBUG,"COULD NOT EXTEND!!!");
					// Tidy the nickname (make case match etc)
					parameter = theuser->nick;
					if (theuser->GetExt(founder, dummyptr))
						log(DEBUG,"Extended!");
					else
						log(DEBUG,"Not extended :(");
					return MODEACTION_ALLOW;
				}
			}
			else
			{
				if (theuser->GetExt(founder, dummyptr))
				{
					theuser->Shrink(founder);
					// Tidy the nickname (make case match etc)
					parameter = theuser->nick;
					return MODEACTION_ALLOW;
				}
			}
			return MODEACTION_DENY;
		}
		else
		{
			// whoops, someones being naughty!
			source->WriteServ("468 %s %s :Only servers may set channel mode +q",source->nick, channel->name);
			parameter = "";
			return MODEACTION_DENY;
		}
	}

	void DisplayList(userrec* user, chanrec* channel)
	{
		CUList* cl = channel->GetUsers();
		std::string founder = "cm_founder_"+std::string(channel->name);
		for (CUList::iterator i = cl->begin(); i != cl->end(); i++)
		{
			if (i->second->GetExt(founder, dummyptr))
			{
				user->WriteServ("386 %s %s %s",user->nick, channel->name,i->second->nick);
			}
		}
		user->WriteServ("387 %s %s :End of channel founder list",user->nick, channel->name);
	}

};

class ChanProtect : public ModeHandler
{
	char* dummyptr;
 public:
	ChanProtect(InspIRCd* Instance) : ModeHandler(Instance, 'a', 1, 1, true, MODETYPE_CHANNEL, false) { }

	ModePair ModeSet(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter)
	{
		userrec* x = ServerInstance->FindNick(parameter);
		if (x)
		{
			if (!channel->HasUser(x))
			{
				return std::make_pair(false, parameter);
			}
			else
			{
				std::string founder = "cm_protect_"+std::string(channel->name);
				if (x->GetExt(founder,dummyptr))
				{
					return std::make_pair(true, x->nick);
				}
				else
				{
					return std::make_pair(false, parameter);
				}
			}
		}
		return std::make_pair(false, parameter);
	}

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		userrec* theuser = ServerInstance->FindNick(parameter);

		// cant find the user given as the parameter, eat the mode change.
		if (!theuser)
		{
			parameter = "";
			return MODEACTION_DENY;
		}

		// given user isnt even on the channel, eat the mode change
		if (!channel->HasUser(theuser))
		{
			parameter = "";
			return MODEACTION_DENY;
		}

		std::string protect = "cm_protect_"+std::string(channel->name);
		std::string founder = "cm_founder_"+std::string(channel->name);

		// source has +q, is a server, or ulined, we'll let them +-a the user.
		if ((is_uline(source->nick)) || (is_uline(source->server)) || (!*source->server) || (source->GetExt(founder,dummyptr)) || (!IS_LOCAL(source)))
		{
			if (adding)
			{
				if (!theuser->GetExt(protect,dummyptr))
				{
					theuser->Extend(protect,fakevalue);
					// Tidy the nickname (make case match etc)
					parameter = theuser->nick;
					return MODEACTION_ALLOW;
				}
			}
			else
			{
				if (theuser->GetExt(protect,dummyptr))
				{
					theuser->Shrink(protect);
					// Tidy the nickname (make case match etc)
					parameter = theuser->nick;
					return MODEACTION_ALLOW;
				}
			}
			return MODEACTION_DENY;
		}
		else
		{
			// bzzzt, wrong answer!
			source->WriteServ("482 %s %s :You are not a channel founder",source->nick, channel->name);
			return MODEACTION_DENY;
		}
	}

	virtual void DisplayList(userrec* user, chanrec* channel)
	{
		CUList* cl = channel->GetUsers();
		std::string protect = "cm_protect_"+std::string(channel->name);
		for (CUList::iterator i = cl->begin(); i != cl->end(); i++)
		{
			if (i->second->GetExt(protect,dummyptr))
			{
				user->WriteServ("388 %s %s %s",user->nick, channel->name,i->second->nick);
			}
		}
		user->WriteServ("389 %s %s :End of channel protected user list",user->nick, channel->name);
	}

};

class ModuleChanProtect : public Module
{
	
	bool FirstInGetsFounder;
	ChanProtect* cp;
	ChanFounder* cf;
	char* dummyptr;
	
 public:
 
	ModuleChanProtect(InspIRCd* Me) : Module::Module(Me)
	{	
		/* Initialise module variables */

		cp = new ChanProtect(ServerInstance);
		cf = new ChanFounder(ServerInstance);

		ServerInstance->AddMode(cp, 'a');
		ServerInstance->AddMode(cf, 'q');
		
		/* Load config stuff */
		OnRehash("");
	}

	void Implements(char* List)
	{
		List[I_On005Numeric] = List[I_OnUserKick] = List[I_OnUserPart] = List[I_OnRehash] = List[I_OnUserJoin] = List[I_OnAccessCheck] = List[I_OnSyncChannel] = 1;
	}
	
	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->ModeGrok->InsertMode(output,"qa",1);
	}

	virtual void OnUserKick(userrec* source, userrec* user, chanrec* chan, const std::string &reason)
	{
		// FIX: when someone gets kicked from a channel we must remove their Extensibles!
		user->Shrink("cm_founder_"+std::string(chan->name));
		user->Shrink("cm_protect_"+std::string(chan->name));
	}

	virtual void OnUserPart(userrec* user, chanrec* channel, const std::string &partreason)
	{
		// FIX: when someone parts a channel we must remove their Extensibles!
		user->Shrink("cm_founder_"+std::string(channel->name));
		user->Shrink("cm_protect_"+std::string(channel->name));
	}

	virtual void OnRehash(const std::string &parameter)
	{
		/* Create a configreader class and read our flag,
		 * in old versions this was heap-allocated and the
		 * object was kept between rehashes...now we just
		 * stack-allocate it locally.
		 */
		ConfigReader Conf(ServerInstance);
		
		FirstInGetsFounder = Conf.ReadFlag("options","noservices",0);
	}
	
	virtual void OnUserJoin(userrec* user, chanrec* channel)
	{
		// if the user is the first user into the channel, mark them as the founder, but only if
		// the config option for it is set
		if (FirstInGetsFounder)
		{
			if (channel->GetUserCounter() == 1)
			{
				// we're using Extensible::Extend to add data into user objects.
				// this way is best as it adds data thats accessible to other modules
				// (so long as you document your code properly) without breaking anything
				// because its encapsulated neatly in a map.

				// Change requested by katsklaw... when the first in is set to get founder,
				// to make it clearer that +q has been given, send that one user the +q notice
				// so that their client's syncronization and their sanity are left intact.
				user->WriteServ("MODE %s +q %s",channel->name,user->nick);
				if (user->Extend("cm_founder_"+std::string(channel->name),fakevalue))
				{
					log(DEBUG,"Marked user "+std::string(user->nick)+" as founder for "+std::string(channel->name));
				}
			}
		}
	}
	
	virtual int OnAccessCheck(userrec* source,userrec* dest,chanrec* channel,int access_type)
	{
		// here we perform access checks, this is the important bit that actually stops kicking/deopping
		// etc of protected users. There are many types of access check, we're going to handle
		// a relatively small number of them relevent to our module using a switch statement.
	
		log(DEBUG,"chanprotect OnAccessCheck %d",access_type);
		// don't allow action if:
		// (A) Theyre founder (no matter what)
		// (B) Theyre protected, and you're not
		// always allow the action if:
		// (A) The source is ulined
		
		
		// firstly, if a ulined nick, or a server, is setting the mode, then allow them to set the mode
		// without any access checks, we're not worthy :p
		if ((ServerInstance->IsUlined(source->nick)) || (ServerInstance->IsUlined(source->server)) || (!strcmp(source->server,"")))
		{
			return ACR_ALLOW;
		}

		std::string founder = "cm_founder_"+std::string(channel->name);
		std::string protect = "cm_protect_"+std::string(channel->name);

		switch (access_type)
		{
			// a user has been deopped. Do we let them? hmmm...
			case AC_DEOP:
				log(DEBUG,"OnAccessCheck AC_DEOP");
				if (dest->GetExt(founder,dummyptr))
				{
					log(DEBUG,"Has %s",founder.c_str());
					source->WriteServ("484 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't deop "+std::string(dest->nick)+" as they're a channel founder");
					return ACR_DENY;
				}
				else
				{
					log(DEBUG,"Doesnt have %s",founder.c_str());
				}
				if ((dest->GetExt(protect,dummyptr)) && (!source->GetExt(protect,dummyptr)))
				{
					source->WriteServ("484 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't deop "+std::string(dest->nick)+" as they're protected (+a)");
					return ACR_DENY;
				}
			break;

			// a user is being kicked. do we chop off the end of the army boot?
			case AC_KICK:
				log(DEBUG,"OnAccessCheck AC_KICK");
				if (dest->GetExt(founder,dummyptr))
				{
					source->WriteServ("484 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't kick "+std::string(dest->nick)+" as they're a channel founder");
					return ACR_DENY;
				}
				if ((dest->GetExt(protect,dummyptr)) && (!source->GetExt(protect,dummyptr)))
				{
					source->WriteServ("484 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't kick "+std::string(dest->nick)+" as they're protected (+a)");
					return ACR_DENY;
				}
			break;

			// a user is being dehalfopped. Yes, we do disallow -h of a +ha user
			case AC_DEHALFOP:
				if (dest->GetExt(founder,dummyptr))
				{
					source->WriteServ("484 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't de-halfop "+std::string(dest->nick)+" as they're a channel founder");
					return ACR_DENY;
				}
				if ((dest->GetExt(protect,dummyptr)) && (!source->GetExt(protect,dummyptr)))
				{
					source->WriteServ("484 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't de-halfop "+std::string(dest->nick)+" as they're protected (+a)");
					return ACR_DENY;
				}
			break;

			// same with devoice.
			case AC_DEVOICE:
				if (dest->GetExt(founder,dummyptr))
				{
					source->WriteServ("484 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't devoice "+std::string(dest->nick)+" as they're a channel founder");
					return ACR_DENY;
				}
				if ((dest->GetExt(protect,dummyptr)) && (!source->GetExt(protect,dummyptr)))
				{
					source->WriteServ("484 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't devoice "+std::string(dest->nick)+" as they're protected (+a)");
					return ACR_DENY;
				}
			break;
		}
		
		// we dont know what this access check is, or dont care. just carry on, nothing to see here.
		return ACR_DEFAULT;
	}
	
	virtual ~ModuleChanProtect()
	{
		DELETE(cp);
		DELETE(cf);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
	}
	
	virtual void OnSyncChannel(chanrec* chan, Module* proto, void* opaque)
	{
		// this is called when the server is linking into a net and wants to sync channel data.
		// we should send our mode changes for the channel here to ensure that other servers
		// know whos +q/+a on the channel.
		CUList* cl = chan->GetUsers();
		string_list commands;
		std::string founder = "cm_founder_"+std::string(chan->name);
		std::string protect = "cm_protect_"+std::string(chan->name);
		for (CUList::iterator i = cl->begin(); i != cl->end(); i++)
		{
			if (i->second->GetExt(founder,dummyptr))
			{
				proto->ProtoSendMode(opaque,TYPE_CHANNEL,chan,"+q "+std::string(i->second->nick));
			}
			if (i->second->GetExt(protect,dummyptr))
			{
				proto->ProtoSendMode(opaque,TYPE_CHANNEL,chan,"+a "+std::string(i->second->nick));
			}
		}
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleChanProtect(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleChanProtectFactory;
}
