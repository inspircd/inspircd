#ifndef INSPIRCD_LISTMODE_PROVIDER
#define INSPIRCD_LISTMODE_PROVIDER

#include <stdio.h>
#include <string>
#include <sstream>
#include <vector>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Provides support for easily creating listmodes, stores the time set, the user, and a parameter. */

/* Written by Om <omster@gmail.com>, December 2005. */
/* Based on code previously written by Om - April 2005 */
/* Originally based on m_chanprotect and m_silence */

inline std::string stringtime()
{
	std::ostringstream TIME;
	TIME << time(NULL); 
	return TIME.str();
}

class ListItem
{
public:
	std::string nick;
	std::string mask;
	std::string time;
};

class ListLimit
{
public:
	std::string mask;
	unsigned int limit;
};

// Just defining the type we use for the excpetion list here...
typedef std::vector<ListItem> modelist;
typedef std::vector<ListLimit> limitlist;

class ListModeBaseModule : public Module
{
protected:
	char mode;
	std::string infokey;
	std::string listnumeric;
	std::string endoflistnumeric;
	std::string endofliststring;
	limitlist chanlimits;

	Server* Srv;
	ConfigReader* Conf;
public:
	ListModeBaseModule(Server* serv, char modechar, std::string eolstr, std::string lnum, std::string eolnum) : Module::Module(serv)
	{
		Srv = serv;
		Conf = new ConfigReader;
		mode = modechar;
		listnumeric = lnum;
		endoflistnumeric = eolnum;
		endofliststring = eolstr;
		
		OnRehash("");
		infokey = "exceptionbase_mode_" + std::string(1, mode) + "_list";
		Srv->AddExtendedListMode(modechar);		
	}
	
	virtual void OnRehash(std::string param)
	{
		delete Conf;
		Conf = new ConfigReader;
		
		chanlimits.clear();
		
		for(int i = 0; i < Conf->Enumerate("banlist"); i++)
		{
			// For each <banlist> tag
			ListLimit limit;
			limit.mask = Conf->ReadValue("banlist", "chan", i);
			limit.limit = Conf->ReadInteger("banlist", "limit", i, true);
			
			if(limit.mask.size() && limit.limit > 0)
			{
				chanlimits.push_back(limit);
				log(DEBUG, "m_exceptionbase.so: Read channel listmode limit of %u for mask '%s'", limit.limit, limit.mask.c_str());
			}
			else
			{
				log(DEBUG, "m_exceptionbase.so: Invalid tag");
			}
		}
		
		if(chanlimits.size() == 0)
		{
			ListLimit limit;
			limit.mask = "*";
			limit.limit = 64;
			chanlimits.push_back(limit);
		}
	}
	
	void DoImplements(char* List)
	{
		List[I_OnExtendedMode] = List[I_OnSendList] = List[I_OnChannelDelete] = List[I_OnSyncChannel] = List[I_OnCleanup] = List[I_OnRehash] = 1;
	}
	
	virtual int OnExtendedMode(userrec *user, void *target, char modechar, int type, bool mode_on, string_list &params)
	{
		// First, check it's our mode
		if ((modechar == mode) && (type == MT_CHANNEL))
		{
			Srv->Log(DEBUG, "m_exceptionbase.so: General listmode handler called, handling mode '" + std::string(1, mode) + "'");
			chanrec* chan = (chanrec*)target;

			// Try and grab the list
			modelist* el = (modelist*)chan->GetExt(infokey);

			if(mode_on)
			{
				// If there was no list
				if(!el)
				{
					// Make one
					Srv->Log(DEBUG, "m_exceptionbase.so: Creating new list");
					el = new modelist;
					chan->Extend(infokey, (char*)el);
				}
				
				if(!Srv->IsValidMask(params[0]))
				{	
					Srv->Log(DEBUG, "m_exceptionbase.so: Banmask was invalid, returning -1");
					return -1;
				}
					
				for (modelist::iterator it = el->begin(); it != el->end(); it++)
				{
					Srv->Log(DEBUG, "m_exceptionbase.so: Iterating over exception list, current mask: " + it->mask);
					if(params[0] == it->mask)
					{
						Srv->Log(DEBUG, "m_exceptionbase.so: Someone tried to set an exception which was already set, returning -1");
						return -1;
					}
				}
				
				unsigned int maxsize = 0;
				
				for(limitlist::iterator it = chanlimits.begin(); it != chanlimits.end(); it++)
				{
					if(Srv->MatchText(chan->name, it->mask))
					{
						// We have a pattern matching the channel...
						maxsize = el->size();
						if(maxsize < it->limit)
						{
							// And now add the mask onto the list...
							ListItem e;
							e.mask = params[0];
							e.nick = user->nick;
							e.time = stringtime();
				
							Srv->Log(DEBUG, "m_exceptionbase.so: All checks passed, adding exception mask to list and returning 1");
							el->push_back(e);
							return 1;
						}
					}
				}

				// List is full
				WriteServ(user->fd, "478 %s %s %s :Channel ban/ignore list is full", user->nick, chan->name, params[0].c_str());
				log(DEBUG, "m_exceptionbase.so: %s tried to set mask %s on %s but the list is full (max %d)", user->nick, params[0].c_str(), chan->name, maxsize);
				return -1;
			}
			else
 			{
				// We're taking the mode off
				if(el)
				{
					for (modelist::iterator it = el->begin(); it != el->end(); it++)
					{
						Srv->Log(DEBUG, "m_exceptionbase.so: Removing mode, iterating over exception list, current mask: " + it->mask);
						if(params[0] == it->mask)
						{
							Srv->Log(DEBUG, "m_exceptionbase.so: Found match for removal of exception, removing and returning 1");
							el->erase(it);
							if(el->size() == 0)
							{
								Srv->Log(DEBUG, "m_exceptionbase.so: Erased the last entry on the exception list, removing the list");
								chan->Shrink(infokey);
								delete el;
							}
							return 1;
						}
					}
					Srv->Log(DEBUG, "m_exceptionbase.so: No match found for attempted removing of exception, returning -1");
					return -1;
				}
				else
				{
					// Hmm, taking an exception off a non-existant list, DIE
					Srv->Log(DEBUG, "m_exceptionbase.so: Attempted removal of an exception, when there was no exception list created, returning -1");
					return -1;
				}
			}
		}
		
		return 0;
	}
	
	virtual void OnSendList(userrec* user, chanrec* chan, char modechar)
	{
		if(modechar == mode)
		{
			modelist* el = (modelist*)chan->GetExt(infokey);
			Srv->Log(DEBUG, "m_exceptionbase.so: " + std::string(user->nick)+" is listing listmodes on "+std::string(chan->name));
			if (el)
			{
				for(modelist::iterator it = el->begin(); it != el->end(); it++)
				{
					WriteServ(user->fd, "%s %s %s %s %s %s", listnumeric.c_str(), user->nick, chan->name, it->mask.c_str(), it->nick.c_str(), it->time.c_str());
				}
			}
			
			WriteServ(user->fd, "%s %s %s %s", endoflistnumeric.c_str(), user->nick, chan->name, endofliststring.c_str());
		}
	}

	virtual void OnChannelDelete(chanrec* chan)
	{
		modelist* list = (modelist*)chan->GetExt(infokey);
			
		if(list)
		{
			chan->Shrink(infokey);
			delete list;
		}
	}
	
	virtual void OnSyncChannel(chanrec* chan, Module* proto, void* opaque)
	{
		modelist* list = (modelist*)chan->GetExt(infokey);		
		if(list)
		{
			for(modelist::iterator it = list->begin(); it != list->end(); it++)
			{
				proto->ProtoSendMode(opaque, TYPE_CHANNEL, chan, "+" + std::string(1, mode) + " " + it->mask);
			}
		}
	}
	
	virtual void OnCleanup(int target_type, void* item)
	{
		if (target_type == TYPE_CHANNEL)
		{
			chanrec* chan = (chanrec*)item;
			
			modelist* list = (modelist*)chan->GetExt(infokey);
			
			if(list)
			{
				chan->Shrink(infokey);
				delete list;
			}
		}
	}
};

#endif
