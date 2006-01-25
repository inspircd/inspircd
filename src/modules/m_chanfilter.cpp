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

/* $ModDesc: Provides channel-specific censor lists (like mode +G but varies from channel to channel) */

typedef std::vector<irc::string> SpamList;

class ModuleChanFilter : public Module
{
	Server *Srv;
	ConfigReader *Conf;
	long MaxEntries;
	
 public:
 
	ModuleChanFilter(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		Conf = new ConfigReader;
		Srv->AddExtendedListMode('g');
		MaxEntries = Conf->ReadInteger("chanfilter","maxsize",0,true);
		if (MaxEntries == 0)
			MaxEntries = 32;
	}

	void Implements(char* List) 
	{ 
		List[I_On005Numeric] = List[I_OnUserPart] = List[I_OnRehash] = List[I_OnUserPreMessage] = List[I_OnUserPreNotice] = List[I_OnExtendedMode] = List[I_OnSendList] = List[I_OnSyncChannel] = 1;
	}
	
        virtual void On005Numeric(std::string &output)
        {
		InsertMode(output,"g",1);
        }

	virtual void OnUserPart(userrec* user, chanrec* channel, std::string partreason)
	{
		// when the last user parts, delete the list
		if (Srv->CountUsers(channel) == 1)
		{
			SpamList* spamlist = (SpamList*)channel->GetExt("spam_list");
			if (spamlist)
			{
				channel->Shrink("spam_list");
				delete spamlist;
			}
		}
	}

	virtual void OnRehash(std::string parameter)
	{
		delete Conf;
		Conf = new ConfigReader;
		// re-read our config options on a rehash
		MaxEntries = Conf->ReadInteger("chanfilter","maxsize",0,true);
	}

        virtual int ProcessMessages(userrec* user,chanrec* chan,std::string &text)
        {

		// Create a copy of the string in irc::string
		irc::string line = text.c_str();

		SpamList* spamlist = (SpamList*)chan->GetExt("spam_list");
		if (spamlist)
		{
			for (SpamList::iterator i = spamlist->begin(); i != spamlist->end(); i++)
			{
				if (line.find(*i) != std::string::npos)
				{
					WriteServ(user->fd,"936 %s %s :Your message contained a censored word, and was blocked",user->nick, chan->name);
					return 1;
				}
			}
		}
		return 0;
	}

	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text)
	{
		if (target_type == TYPE_CHANNEL)
		{
			return ProcessMessages(user,(chanrec*)dest,text);
		}
		else return 0;
	}

	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text)
	{
		if (target_type == TYPE_CHANNEL)
		{
			return ProcessMessages(user,(chanrec*)dest,text);
		}
		else return 0;
	}
	
	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		if ((modechar == 'g') && (type == MT_CHANNEL))
		{
			chanrec* chan = (chanrec*)target;

			irc::string word = params[0].c_str();

			if (mode_on)
			{
				SpamList* spamlist = (SpamList*)chan->GetExt("spam_list");
				if (!spamlist)
				{
					spamlist = new SpamList;
					chan->Extend("spam_list",(char*)spamlist);
				}
				if (spamlist->size() < (unsigned)MaxEntries)
				{
					for (SpamList::iterator i = spamlist->begin(); i != spamlist->end(); i++)
					{
						if (*i == word)
						{
							WriteServ(user->fd,"937 %s %s :The word %s is already on the spamfilter list",user->nick, chan->name,word.c_str());
							return -1;
						}
					}
					spamlist->push_back(word);
					return 1;
				}
				WriteServ(user->fd,"939 %s %s :Channel spamfilter list is full",user->nick, chan->name);
				return -1;
			}
			else
			{
				SpamList* spamlist = (SpamList*)chan->GetExt("spam_list");
				if (spamlist)
				{
					for (SpamList::iterator i = spamlist->begin(); i != spamlist->end(); i++)
					{
						if (*i == word)
						{
							spamlist->erase(i);
							return 1;
						}
					}
				}
				WriteServ(user->fd,"938 %s %s :No such spamfilter word is set",user->nick, chan->name);
				return -1;
			}
			return -1;
		}	
		return 0;
	}

	virtual void OnSendList(userrec* user, chanrec* channel, char mode)
	{
		if (mode == 'g')
		{
			SpamList* spamlist = (SpamList*)channel->GetExt("spam_list");
			if (spamlist)
			{
				for (SpamList::iterator i = spamlist->begin(); i != spamlist->end(); i++)
				{
					WriteServ(user->fd,"941 %s %s %s",user->nick, channel->name,i->c_str());
				}
			}
			WriteServ(user->fd,"940 %s %s :End of channel spamfilter list",user->nick, channel->name);
		}
	}
	
	virtual ~ModuleChanFilter()
	{
		delete Conf;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
	}
	
	virtual void OnSyncChannel(chanrec* chan, Module* proto, void* opaque)
	{
		SpamList* spamlist = (SpamList*)chan->GetExt("spam_list");
		string_list commands;
		if (spamlist)
		{
			for (SpamList::iterator i = spamlist->begin(); i != spamlist->end(); i++)
			{
				proto->ProtoSendMode(opaque,TYPE_CHANNEL,chan,"+g "+std::string(i->c_str()));
			}
		}
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
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleChanFilter(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleChanFilterFactory;
}

