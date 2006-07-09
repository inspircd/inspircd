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
#include "helperfuncs.h"

/* $ModDesc: Provides channel modes +a and +q */

const char fakevalue* = "on";

class ChanFounder : public ModeHandler
{
 public:
	ChanFounder() : ModeHandler('q', 1, 1, true, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		userrec* theuser = Srv->FindNick(parameter);

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

		 // source is a server, or ulined, we'll let them +-q the user.
		if ((Srv->IsUlined(source->nick)) || (Srv->IsUlined(source->server)) || (!*source->server))
		{
			if (adding)
			{
				if (!theuser->GetExt("cm_founder_"+std::string(channel->name)))
				{
					theuser->Extend("cm_founder_"+std::string(channel->name),fakevalue);
					// Tidy the nickname (make case match etc)
					parameter = theuser->nick;
					return MODEACTION_ALLOW;
				}
			}
			else
			{
				if (theuser->GetExt("cm_founder_"+std::string(channel->name)))
				{
					theuser->Shrink("cm_founder_"+std::string(channel->name));
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
			WriteServ(source->fd,"468 %s %s :Only servers may set channel mode +q",source->nick, channel->name);
			parameter = "";
			return MODEACTION_DENY;
		}
	}

	void DisplayList(userrec* user, chanrec* channel)
	{
		chanuserlist cl = Srv->GetUsers(channel);
		for (unsigned int i = 0; i < cl.size(); i++)
		{
			if (cl[i]->GetExt("cm_founder_"+std::string(channel->name)))
			{
				WriteServ(user->fd,"386 %s %s %s",user->nick, channel->name,cl[i]->nick);
			}
		}
		WriteServ(user->fd,"387 %s %s :End of channel founder list",user->nick, channel->name);
	}

};

class ChanProtect : public ModeHandler
{
 public:
	ChanProtect() : ModeHandler('a', 1, 1, true, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		userrec* theuser = Srv->FindNick(parameter);

		// cant find the user given as the parameter, eat the mode change.
		if (!theuser)
		{
			parameter = "";
			return MODEACTION_DENY;
		}

		// given user isnt even on the channel, eat the mode change
		if (!chan->HasUser(theuser))
		{
			parameter = "";
			return MODEACTION_DENY;
		}

		// source has +q, is a server, or ulined, we'll let them +-a the user.
		if ((Srv->IsUlined(source->nick)) || (Srv->IsUlined(source->server)) || (!*source->server) || (source->GetExt("cm_founder_"+std::string(channel->name))))
		{
			if (adding)
			{
				if (!theuser->GetExt("cm_protect_"+std::string(channel->name)))
				{
					theuser->Extend("cm_protect_"+std::string(channel->name),fakevalue);
					// Tidy the nickname (make case match etc)
					parameter = theuser->nick;
					return MODEACTION_ALLOW;
				}
			}
			else
			{
				if (theuser->GetExt("cm_protect_"+std::string(channel->name)))
				{
					theuser->Shrink("cm_protect_"+std::string(channel->name));
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
			WriteServ(user->fd,"482 %s %s :You are not a channel founder",user->nick, chan->name);
			return MODEACTION_DENY;
		}
	}

	virtual void DisplayList(userrec* user, chanrec* channel)
	{
		chanuserlist cl = Srv->GetUsers(channel);
		for (unsigned int i = 0; i < cl.size(); i++)
		{
			if (cl[i]->GetExt("cm_protect_"+std::string(channel->name)))
			{
				WriteServ(user->fd,"388 %s %s %s",user->nick, channel->name,cl[i]->nick);
			}
		}
		WriteServ(user->fd,"389 %s %s :End of channel protected user list",user->nick, channel->name);
	}

};

class ModuleChanProtect : public Module
{
	Server *Srv;
	bool FirstInGetsFounder;
	ConfigReader *Conf;
	ChanProtect* cp;
	ChanFounder* cf;
	
 public:
 
	ModuleChanProtect(Server* Me)
	: Module::Module(Me), Srv(Me)
	{	
		/* Initialise module variables */
		Conf = new ConfigReader;

		cp = new ChanProtect();
		cf = new ChanFounder();
		
		Srv->AddMode(cp, 'a');
		Srv->AdDMode(cf, 'q');
		
		// read our config options (main config file)
		FirstInGetsFounder = Conf->ReadFlag("options","noservices",0);
	}

	void Implements(char* List)
	{
		List[I_On005Numeric] = List[I_OnUserKick] = List[I_OnUserPart] = List[I_OnRehash] = List[I_OnUserJoin] = List[I_OnAccessCheck] = List[I_OnSyncChannel] = 1;
	}
	
	virtual void On005Numeric(std::string &output)
	{
		InsertMode(output,"qa",1);
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
		// on a rehash we delete our classes for good measure and create them again.
		DELETE(Conf);
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

				// Change requested by katsklaw... when the first in is set to get founder,
				// to make it clearer that +q has been given, send that one user the +q notice
				// so that their client's syncronization and their sanity are left intact.
				WriteServ(user->fd,"MODE %s +q %s",channel->name,user->nick);
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
					Srv->SendServ(source->fd,"484 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't deop "+std::string(dest->nick)+" as they're a channel founder");
					return ACR_DENY;
				}
				if ((dest->GetExt("cm_protect_"+std::string(channel->name))) && (!source->GetExt("cm_protect_"+std::string(channel->name))))
				{
					Srv->SendServ(source->fd,"484 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't deop "+std::string(dest->nick)+" as they're protected (+a)");
					return ACR_DENY;
				}
			break;

			// a user is being kicked. do we chop off the end of the army boot?
			case AC_KICK:
				if (dest->GetExt("cm_founder_"+std::string(channel->name)))
				{
					Srv->SendServ(source->fd,"484 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't kick "+std::string(dest->nick)+" as they're a channel founder");
					return ACR_DENY;
				}
				if ((dest->GetExt("cm_protect_"+std::string(channel->name))) && (!source->GetExt("cm_protect_"+std::string(channel->name))))
				{
					Srv->SendServ(source->fd,"484 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't kick "+std::string(dest->nick)+" as they're protected (+a)");
					return ACR_DENY;
				}
			break;

			// a user is being dehalfopped. Yes, we do disallow -h of a +ha user
			case AC_DEHALFOP:
				if (dest->GetExt("cm_founder_"+std::string(channel->name)))
				{
					Srv->SendServ(source->fd,"484 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't de-halfop "+std::string(dest->nick)+" as they're a channel founder");
					return ACR_DENY;
				}
				if ((dest->GetExt("cm_protect_"+std::string(channel->name))) && (!source->GetExt("cm_protect_"+std::string(channel->name))))
				{
					Srv->SendServ(source->fd,"484 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't de-halfop "+std::string(dest->nick)+" as they're protected (+a)");
					return ACR_DENY;
				}
			break;

			// same with devoice.
			case AC_DEVOICE:
				if (dest->GetExt("cm_founder_"+std::string(channel->name)))
				{
					Srv->SendServ(source->fd,"484 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't devoice "+std::string(dest->nick)+" as they're a channel founder");
					return ACR_DENY;
				}
				if ((dest->GetExt("cm_protect_"+std::string(channel->name))) && (!source->GetExt("cm_protect_"+std::string(channel->name))))
				{
					Srv->SendServ(source->fd,"484 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't devoice "+std::string(dest->nick)+" as they're protected (+a)");
					return ACR_DENY;
				}
			break;
		}
		
		// we dont know what this access check is, or dont care. just carry on, nothing to see here.
		return ACR_DEFAULT;
	}
	
	virtual ~ModuleChanProtect()
	{
		DELETE(Conf);
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
		chanuserlist cl = Srv->GetUsers(chan);
		string_list commands;
		for (unsigned int i = 0; i < cl.size(); i++)
		{
			if (cl[i]->GetExt("cm_founder_"+std::string(chan->name)))
			{
				proto->ProtoSendMode(opaque,TYPE_CHANNEL,chan,"+q "+std::string(cl[i]->nick));
			}
			if (cl[i]->GetExt("cm_protect_"+std::string(chan->name)))
			{
				proto->ProtoSendMode(opaque,TYPE_CHANNEL,chan,"+a "+std::string(cl[i]->nick));
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
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleChanProtect(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleChanProtectFactory;
}
