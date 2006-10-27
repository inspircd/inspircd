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
#include "inspircd.h"

/* $ModDesc: Provides channel modes +a and +q */
/* $ModDep: ../../include/u_listmode.h */

#define PROTECT_VALUE 40000
#define FOUNDER_VALUE 50000

const char* fakevalue = "on";

/* When this is set to true, no restrictions apply to setting or
 * removal of +qa. This is used while unloading so that the server
 * can freely clear all of its users of the modes.
 */
bool unload_kludge = false;

/** Handles basic operation of +qa channel modes
 */
class FounderProtectBase
{
 private:
	InspIRCd* MyInstance;
	std::string extend;
	std::string type;
	int list;
	int end;
	char* dummyptr;
 public:
	FounderProtectBase(InspIRCd* Instance, const std::string &ext, const std::string &mtype, int l, int e) : MyInstance(Instance), extend(ext), type(mtype), list(l), end(e)
	{
	}

	ModePair ModeSet(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter)
	{
		userrec* x = MyInstance->FindNick(parameter);
		if (x)
		{
			if (!channel->HasUser(x))
			{
				return std::make_pair(false, parameter);
			}
			else
			{
				std::string item = extend+std::string(channel->name);
				if (x->GetExt(item,dummyptr))
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

	void RemoveMode(chanrec* channel, char mc)
	{
		unload_kludge = true;
		CUList* cl = channel->GetUsers();
		std::string item = extend + std::string(channel->name);
		const char* mode_junk[MAXMODES+1];
		userrec* n = new userrec(MyInstance);
		n->SetFd(FD_MAGIC_NUMBER);
		mode_junk[0] = channel->name;
		irc::modestacker modestack(false);
		std::deque<std::string> stackresult;				
		for (CUList::iterator i = cl->begin(); i != cl->end(); i++)
		{
			if (i->second->GetExt(item, dummyptr))
			{
				modestack.Push(mc, i->second->nick);
			}
		}

		while (modestack.GetStackedLine(stackresult))
		{
			for (size_t j = 0; j < stackresult.size(); j++)
			{
				mode_junk[j+1] = stackresult[j].c_str();
			}
			MyInstance->SendMode(mode_junk, stackresult.size() + 1, n);
		}
		
		delete n;
		unload_kludge = false;
	}

        void DisplayList(userrec* user, chanrec* channel)
	{
		CUList* cl = channel->GetUsers();
		std::string item = extend+std::string(channel->name);
		for (CUList::iterator i = cl->begin(); i != cl->end(); i++)
		{
			if (i->second->GetExt(item, dummyptr))
			{
				user->WriteServ("%d %s %s %s", list, user->nick, channel->name,i->second->nick);
			}
		}
		user->WriteServ("%d %s %s :End of channel %s list", end, user->nick, channel->name, type.c_str());
	}

	userrec* FindAndVerify(std::string &parameter, chanrec* channel)
	{
		userrec* theuser = MyInstance->FindNick(parameter);
		if ((!theuser) || (!channel->HasUser(theuser)))
		{
			parameter = "";
			return NULL;
		}
		return theuser;
	}

	ModeAction HandleChange(userrec* source, userrec* theuser, bool adding, chanrec* channel, std::string &parameter)
	{
		std::string item = extend+std::string(channel->name);

		if (adding)
		{
			if (!theuser->GetExt(item, dummyptr))
			{
				theuser->Extend(item, fakevalue);
				parameter = theuser->nick;
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (theuser->GetExt(item, dummyptr))
			{
				theuser->Shrink(item);
				parameter = theuser->nick;
				return MODEACTION_ALLOW;
			}
		}
		return MODEACTION_DENY;
	}
};

/** Abstraction of FounderProtectBase for channel mode +q
 */
class ChanFounder : public ModeHandler, public FounderProtectBase
{
	char* dummyptr;
 public:
	ChanFounder(InspIRCd* Instance, bool using_prefixes)
		: ModeHandler(Instance, 'q', 1, 1, true, MODETYPE_CHANNEL, false, using_prefixes ? '~' : 0),
		  FounderProtectBase(Instance, "cm_founder_", "founder", 386, 387) { }

	unsigned int GetPrefixRank()
	{
		return FOUNDER_VALUE;
	}

	ModePair ModeSet(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter)
	{
		return FounderProtectBase::ModeSet(source, dest, channel, parameter);
	}

	void RemoveMode(chanrec* channel)
	{
		FounderProtectBase::RemoveMode(channel, this->GetModeChar());
	}

	void RemoveMode(userrec* user)
	{
	}

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		userrec* theuser = FounderProtectBase::FindAndVerify(parameter, channel);

		if (!theuser)
		{
			return MODEACTION_DENY;
		}

		 // source is a server, or ulined, we'll let them +-q the user.
		if ((unload_kludge) || (ServerInstance->ULine(source->nick)) || (ServerInstance->ULine(source->server)) || (!*source->server) || (!IS_LOCAL(source)))
		{
			return FounderProtectBase::HandleChange(source, theuser, adding, channel, parameter);
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
		FounderProtectBase::DisplayList(user,channel);
	}
};

/** Abstraction of FounderProtectBase for channel mode +a
 */
class ChanProtect : public ModeHandler, public FounderProtectBase
{
	char* dummyptr;
 public:
	ChanProtect(InspIRCd* Instance, bool using_prefixes)
		: ModeHandler(Instance, 'a', 1, 1, true, MODETYPE_CHANNEL, false, using_prefixes ? '&' : 0),
		  FounderProtectBase(Instance,"cm_protect_","protected user", 388, 389) { }

	unsigned int GetPrefixRank()
	{
		return PROTECT_VALUE;
	}

	ModePair ModeSet(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter)
	{
		return FounderProtectBase::ModeSet(source, dest, channel, parameter);
	}

	void RemoveMode(chanrec* channel)
	{
		FounderProtectBase::RemoveMode(channel, this->GetModeChar());
	}

	void RemoveMode(userrec* user)
	{
	}

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		userrec* theuser = FounderProtectBase::FindAndVerify(parameter, channel);

		if (!theuser)
			return MODEACTION_DENY;

		std::string founder = "cm_founder_"+std::string(channel->name);

		// source has +q, is a server, or ulined, we'll let them +-a the user.
		if ((unload_kludge) || (ServerInstance->ULine(source->nick)) || (ServerInstance->ULine(source->server)) || (!*source->server) || (source->GetExt(founder,dummyptr)) || (!IS_LOCAL(source)))
		{
			return FounderProtectBase::HandleChange(source, theuser, adding, channel, parameter);
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
		FounderProtectBase::DisplayList(user, channel);
	}

};

class ModuleChanProtect : public Module
{
	
	bool FirstInGetsFounder;
	bool QAPrefixes;
	bool booting;
	ChanProtect* cp;
	ChanFounder* cf;
	char* dummyptr;
	
 public:
 
	ModuleChanProtect(InspIRCd* Me) : Module::Module(Me)
	{	
		/* Load config stuff */
		booting = true;
		OnRehash("");
		booting = false;

		/* Initialise module variables */

		cp = new ChanProtect(ServerInstance,QAPrefixes);
		cf = new ChanFounder(ServerInstance,QAPrefixes);

		ServerInstance->AddMode(cp, 'a');
		ServerInstance->AddMode(cf, 'q');
	}

	void Implements(char* List)
	{
		List[I_OnUserKick] = List[I_OnUserPart] = List[I_OnRehash] = List[I_OnUserJoin] = List[I_OnAccessCheck] = List[I_OnSyncChannel] = 1;
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

		bool old_qa = QAPrefixes;
		
		FirstInGetsFounder = Conf.ReadFlag("options","noservices",0);
		QAPrefixes = Conf.ReadFlag("options","qaprefixes",0);

		/* Did the user change the QA prefixes on the fly?
		 * If so, remove all instances of the mode, and reinit
		 * the module with prefixes enabled.
		 */
		if ((old_qa != QAPrefixes) && (!booting))
		{
			ServerInstance->Modes->DelMode(cp);
			ServerInstance->Modes->DelMode(cf);
			DELETE(cp);
			DELETE(cf);
			cp = new ChanProtect(ServerInstance,QAPrefixes);
			cf = new ChanFounder(ServerInstance,QAPrefixes);
			ServerInstance->AddMode(cp, 'a');
			ServerInstance->AddMode(cf, 'q');
			ServerInstance->WriteOpers("*** WARNING: +qa prefixes were enabled or disabled via a REHASH. Clients will probably need to reconnect to pick up this change.");
		}
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
					ServerInstance->Log(DEBUG,"Marked user "+std::string(user->nick)+" as founder for "+std::string(channel->name));
				}
			}
		}
	}
	
	virtual int OnAccessCheck(userrec* source,userrec* dest,chanrec* channel,int access_type)
	{
		// here we perform access checks, this is the important bit that actually stops kicking/deopping
		// etc of protected users. There are many types of access check, we're going to handle
		// a relatively small number of them relevent to our module using a switch statement.
	
		ServerInstance->Log(DEBUG,"chanprotect OnAccessCheck %d",access_type);
		// don't allow action if:
		// (A) Theyre founder (no matter what)
		// (B) Theyre protected, and you're not
		// always allow the action if:
		// (A) The source is ulined
		
		
		// firstly, if a ulined nick, or a server, is setting the mode, then allow them to set the mode
		// without any access checks, we're not worthy :p
		if ((ServerInstance->ULine(source->nick)) || (ServerInstance->ULine(source->server)) || (!*source->server))
		{
			ServerInstance->Log(DEBUG,"chanprotect OnAccessCheck returns ALLOW");
			return ACR_ALLOW;
		}

		std::string founder = "cm_founder_"+std::string(channel->name);
		std::string protect = "cm_protect_"+std::string(channel->name);

		switch (access_type)
		{
			// a user has been deopped. Do we let them? hmmm...
			case AC_DEOP:
				ServerInstance->Log(DEBUG,"OnAccessCheck AC_DEOP");
				if (dest->GetExt(founder,dummyptr))
				{
					ServerInstance->Log(DEBUG,"Has %s",founder.c_str());
					source->WriteServ("484 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't deop "+std::string(dest->nick)+" as they're a channel founder");
					return ACR_DENY;
				}
				else
				{
					ServerInstance->Log(DEBUG,"Doesnt have %s",founder.c_str());
				}
				if ((dest->GetExt(protect,dummyptr)) && (!source->GetExt(protect,dummyptr)))
				{
					source->WriteServ("484 "+std::string(source->nick)+" "+std::string(channel->name)+" :Can't deop "+std::string(dest->nick)+" as they're protected (+a)");
					return ACR_DENY;
				}
			break;

			// a user is being kicked. do we chop off the end of the army boot?
			case AC_KICK:
				ServerInstance->Log(DEBUG,"OnAccessCheck AC_KICK");
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
		ServerInstance->Log(DEBUG,"chanprotect OnAccessCheck returns DEFAULT");
		return ACR_DEFAULT;
	}
	
	virtual ~ModuleChanProtect()
	{
		ServerInstance->Modes->DelMode(cp);
		ServerInstance->Modes->DelMode(cf);
		DELETE(cp);
		DELETE(cf);
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
	}
	
	virtual void OnSyncChannel(chanrec* chan, Module* proto, void* opaque)
	{
		/* NOTE: If +qa prefix is on, this is propogated by the channel join,
		 * so we dont need to propogate it manually
		 */
		if (!QAPrefixes)
		{
			// this is called when the server is linking into a net and wants to sync channel data.
			// we should send our mode changes for the channel here to ensure that other servers
			// know whos +q/+a on the channel.
			CUList* cl = chan->GetUsers();
			string_list commands;
			std::string founder = "cm_founder_"+std::string(chan->name);
			std::string protect = "cm_protect_"+std::string(chan->name);
			irc::modestacker modestack(true);
			std::deque<std::string> stackresult;
			for (CUList::iterator i = cl->begin(); i != cl->end(); i++)
			{
				if (i->second->GetExt(founder,dummyptr))
				{
					modestack.Push('q',i->second->nick);
				}
				if (i->second->GetExt(protect,dummyptr))
				{
					modestack.Push('a',i->second->nick);
				}
			}
			while (modestack.GetStackedLine(stackresult))
			{
				irc::stringjoiner mode_join(" ", stackresult, 0, stackresult.size() - 1);
				std::string line = mode_join.GetJoined();
				proto->ProtoSendMode(opaque,TYPE_CHANNEL,chan, line);
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
