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
#include <string>
#include <vector>
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides channel-specific censor lists (like mode +G but varies from channel to channel) */

typedef std::vector<std::string> SpamList;

class ModuleChanFilter : public Module
{
	Server *Srv;
	ConfigReader *Conf;
	long MaxEntries;
	
 public:
 
	ModuleChanFilter()
	{
		Srv = new Server;
		Conf = new ConfigReader;
		Srv->AddExtendedListMode('g');
		MaxEntries = Conf->ReadInteger("chanfilter","maxsize",0,true);
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
                                temp1 = "CHANMODES=g" + temp1;
                        }
                        temp2 = temp2 + temp1 + " ";
                }
		if (temp2.length())
	                output = temp2.substr(0,temp2.length()-1);
        }

	virtual void OnUserPart(userrec* user, chanrec* channel)
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

	virtual void OnRehash()
	{
		delete Conf;
		Conf = new ConfigReader;
		// re-read our config options on a rehash
		MaxEntries = Conf->ReadInteger("chanfilter","maxsize",0,true);
	}

        virtual int ProcessMessages(userrec* user,chanrec* chan,std::string &text)
        {
		SpamList* spamlist = (SpamList*)chan->GetExt("spam_list");
		if (spamlist)
		{
			for (SpamList::iterator i = spamlist->begin(); i != spamlist->end(); i++)
			{
				if (strcasestr(text.c_str(),i->c_str()))
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
			std::string param = params[0];
		
			if (mode_on)
			{
				SpamList* spamlist = (SpamList*)chan->GetExt("spam_list");
				if (!spamlist)
				{
					spamlist = new SpamList;
					chan->Extend("spam_list",(char*)spamlist);
				}
				if (spamlist->size() < MaxEntries)
				{
					for (SpamList::iterator i = spamlist->begin(); i != spamlist->end(); i++)
					{
						if (*i == params[0])
						{
							WriteServ(user->fd,"937 %s %s :The word %s is already on the spamfilter list",user->nick, chan->name,params[0].c_str());
							return -1;
						}
					}
					spamlist->push_back(params[0]);
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
						if (*i == params[0])
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
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
	}
	
	virtual string_list OnChannelSync(chanrec* chan)
	{
		SpamList* spamlist = (SpamList*)chan->GetExt("spam_list");
		string_list commands;
		if (spamlist)
		{
			for (SpamList::iterator i = spamlist->begin(); i != spamlist->end(); i++)
			{
				commands.push_back("M "+std::string(chan->name)+" +g "+*i);
			}
		}
		return commands;
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
	
	virtual Module * CreateModule()
	{
		return new ModuleChanFilter;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleChanFilterFactory;
}

