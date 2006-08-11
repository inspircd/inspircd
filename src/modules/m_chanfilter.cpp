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

using namespace std;

#include <stdio.h>
#include <string>
#include <vector>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "u_listmode.h"
#include "inspircd.h"

/* $ModDesc: Provides channel-specific censor lists (like mode +G but varies from channel to channel) */



class ChanFilter : public ListModeBase
{
 public:
	ChanFilter(InspIRCd* Instance) : ListModeBase(Instance, 'g', "End of channel spamfilter list", "941", "940", false, "chanfilter") { }
	
	virtual bool ValidateParam(userrec* user, chanrec* chan, std::string &word)
	{
		if (word.length() > 35)
		{
			user->WriteServ( "935 %s %s %s :word is too long for censor list",user->nick, chan->name,word.c_str());
			return false;
		}
		
		return true;
	}
	
	virtual bool TellListTooLong(userrec* user, chanrec* chan, std::string &word)
	{
		user->WriteServ("939 %s %s %s :Channel spamfilter list is full",user->nick, chan->name, word.c_str());
		return true;
	}
	
	virtual void TellAlreadyOnList(userrec* user, chanrec* chan, std::string &word)
	{
		user->WriteServ("937 %s %s :The word %s is already on the spamfilter list",user->nick, chan->name,word.c_str());
	}
	
	virtual void TellNotSet(userrec* user, chanrec* chan, std::string &word)
	{
		user->WriteServ("938 %s %s :No such spamfilter word is set",user->nick, chan->name);
	}
};

class ModuleChanFilter : public Module
{
	
	ChanFilter* cf;
	
 public:
 
	ModuleChanFilter(InspIRCd* Me)
		: Module::Module(Me)
	{
		cf = new ChanFilter(ServerInstance);
		ServerInstance->AddMode(cf, 'g');
	}

	void Implements(char* List) 
	{ 
		cf->DoImplements(List);
		List[I_OnCleanup] = List[I_On005Numeric] = List[I_OnChannelDelete] = List[I_OnRehash] = List[I_OnUserPreMessage] = List[I_OnUserPreNotice] = List[I_OnSyncChannel] = 1;
	}
	
	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->Modes->InsertMode(output,"g",1);
	}

	virtual void OnChannelDelete(chanrec* chan)
	{
		cf->DoChannelDelete(chan);
	}

	virtual void OnRehash(const std::string &parameter)
	{
		cf->DoRehash();
	}

	virtual int ProcessMessages(userrec* user,chanrec* chan,std::string &text)
	{
		// Create a copy of the string in irc::string
		irc::string line = text.c_str();

		modelist* list;
		chan->GetExt(cf->GetInfoKey(), list);

		if (list)
		{
			for (modelist::iterator i = list->begin(); i != list->end(); i++)
			{
				if (line.find(i->mask.c_str()) != std::string::npos)
				{
					user->WriteServ("936 %s %s %s :Your message contained a censored word, and was blocked",user->nick, chan->name, i->mask.c_str());
					return 1;
				}
			}
		}
		return 0;
	}

	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		if (target_type == TYPE_CHANNEL)
		{
			return ProcessMessages(user,(chanrec*)dest,text);
		}
		else return 0;
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		cf->DoCleanup(target_type, item);
	}
	
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		return OnUserPreMessage(user,dest,target_type,text,status);
	}
	
	virtual void OnSyncChannel(chanrec* chan, Module* proto, void* opaque)
	{
		cf->DoSyncChannel(chan, proto, opaque);
	}

	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_STATIC|VF_VENDOR);
	}
	
	virtual ~ModuleChanFilter()
	{
		DELETE(cf);
	}
};


class ModuleChanFilterFactory : public ModuleFactory
{
 public:
	ModuleChanFilterFactory()
	{
	}
	
	~ModuleChanFilterFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleChanFilter(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleChanFilterFactory;
}
