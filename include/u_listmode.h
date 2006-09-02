#ifndef INSPIRCD_LISTMODE_PROVIDER
#define INSPIRCD_LISTMODE_PROVIDER

#include <stdio.h>
#include <string>
#include <sstream>
#include <vector>
#include "users.h"
#include "channels.h"
#include "modules.h"

#include "wildcard.h"
#include "inspircd.h"

/* $ModDesc: Provides support for easily creating listmodes, stores the time set, the user, and a parameter. */

/* Updated to use the <banlist> config tag if it exists */
/* Written by Om <omster@gmail.com>, December 2005. */
/* Based on code previously written by Om - April 2005 */
/* Updated to new API July 8th 2006 by Brain */
/* Originally based on m_chanprotect and m_silence */

inline std::string stringtime()
{
	std::ostringstream TIME;
	TIME << time(NULL); 
	return TIME.str();
}

class ListItem : public classbase
{
public:
	std::string nick;
	std::string mask;
	std::string time;
};

class ListLimit : public classbase
{
public:
	std::string mask;
	unsigned int limit;
};

// Just defining the type we use for the exception list here...
typedef std::vector<ListItem> modelist;
typedef std::vector<ListLimit> limitlist;

class ListModeBase : public ModeHandler
{
 protected:
	std::string infokey;
	std::string listnumeric;
	std::string endoflistnumeric;
	std::string endofliststring;
	bool tidy;
 	std::string configtag;
	limitlist chanlimits;
 
 public:
	ListModeBase(InspIRCd* Instance, char modechar, const std::string &eolstr, const std::string &lnum, const std::string &eolnum, bool autotidy, const std::string &ctag = "banlist")
 	: ModeHandler(Instance, modechar, 1, 1, true, MODETYPE_CHANNEL, false), listnumeric(lnum), endoflistnumeric(eolnum), endofliststring(eolstr), tidy(autotidy), configtag(ctag)
	{
		this->DoRehash();
		infokey = "exceptionbase_mode_" + std::string(1, mode) + "_list";
	}

	std::pair<bool,std::string> ModeSet(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter)
	{
		modelist* el;
		channel->GetExt(infokey, el);
		if (el)
		{
			for (modelist::iterator it = el->begin(); it != el->end(); it++)
			{
				if(parameter == it->mask)
				{
					return std::make_pair(true, parameter);
				}
			}
		}
		return std::make_pair(false, parameter);
	}

	virtual void DisplayList(userrec* user, chanrec* channel)
	{
		modelist* el;
		channel->GetExt(infokey, el);
		if (el)
		{
			for(modelist::iterator it = el->begin(); it != el->end(); it++)
			{
				user->WriteServ("%s %s %s %s %s %s", listnumeric.c_str(), user->nick, channel->name, it->mask.c_str(), it->nick.c_str(), it->time.c_str());
			}
		}
		user->WriteServ("%s %s %s %s", endoflistnumeric.c_str(), user->nick, channel->name, endofliststring.c_str());
	}

	virtual void RemoveMode(chanrec* channel)
	{
		ServerInstance->Log(DEBUG,"Removing listmode base from %s %s",channel->name,infokey.c_str());
		modelist* el;
		channel->GetExt(infokey, el);
		if (el)
		{
			ServerInstance->Log(DEBUG,"Channel is extended with a list");
			char moderemove[MAXBUF];
			userrec* n = new userrec(ServerInstance);
			n->SetFd(FD_MAGIC_NUMBER);
			for(modelist::iterator it = el->begin(); it != el->end(); it++)
			{
				ServerInstance->Log(DEBUG,"Remove item %s",it->mask.c_str());
				sprintf(moderemove,"-%c",this->GetModeChar());
				const char* parameters[] = { channel->name, moderemove, it->mask.c_str() };
				ServerInstance->SendMode(parameters, 3, n);
			}
			delete n;
		}
	}

	virtual void RemoveMode(userrec* user)
	{
		/* Listmodes dont get set on users */
	}

	virtual void DoRehash()
	{
		ConfigReader Conf(ServerInstance);

		chanlimits.clear();

		for(int i = 0; i < Conf.Enumerate(configtag); i++)
		{
			// For each <banlist> tag
			ListLimit limit;
			limit.mask = Conf.ReadValue(configtag, "chan", i);
			limit.limit = Conf.ReadInteger(configtag, "limit", i, true);

			if(limit.mask.size() && limit.limit > 0)
				chanlimits.push_back(limit);
		}
		if(chanlimits.size() == 0)
		{
			ListLimit limit;
			limit.mask = "*";
			limit.limit = 64;
			chanlimits.push_back(limit);
		}
	}

	virtual void DoImplements(char* List)
	{
		List[I_OnChannelDelete] = List[I_OnSyncChannel] = List[I_OnCleanup] = List[I_OnRehash] = 1;
	}

	virtual ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		// Try and grab the list
		modelist* el;
		channel->GetExt(infokey, el);

		if (adding)
		{
			// If there was no list
			if (!el)
			{
				// Make one
				el = new modelist;
				channel->Extend(infokey, el);
			}

			// Clean the mask up
			if (this->tidy)
				ModeParser::CleanMask(parameter);

			// Check if the item already exists in the list
			for (modelist::iterator it = el->begin(); it != el->end(); it++)
			{
				if(parameter == it->mask)
				{
					/* Give a subclass a chance to error about this */
					TellAlreadyOnList(source, channel, parameter);
					
					// it does, deny the change
					return MODEACTION_DENY;
				}
			}

			unsigned int maxsize = 0;

			for (limitlist::iterator it = chanlimits.begin(); it != chanlimits.end(); it++)
			{
				if (match(channel->name, it->mask.c_str()))
				{
					// We have a pattern matching the channel...
					maxsize = el->size();
					if (maxsize < it->limit)
					{
						/* Ok, it *could* be allowed, now give someone subclassing us
						 * a chance to validate the parameter.
						 * The param is passed by reference, so they can both modify it
						 * and tell us if we allow it or not.
						 *
						 * eg, the subclass could:
						 * 1) allow
						 * 2) 'fix' parameter and then allow
						 * 3) deny
						 */
						if(ValidateParam(source, channel, parameter))
						{
							// And now add the mask onto the list...
							ListItem e;
							e.mask = parameter;
							e.nick = source->nick;
							e.time = stringtime();

							el->push_back(e);
							return MODEACTION_ALLOW;
						}
						else
						{
							/* If they deny it they have the job of giving an error message */
							return MODEACTION_DENY;
						}
					}
				}
			}

			/* List is full, give subclass a chance to send a custom message */
			if(!TellListTooLong(source, channel, parameter))
			{
				source->WriteServ("478 %s %s %s :Channel ban/ignore list is full", source->nick, channel->name, parameter.c_str());
			}
			
			parameter = "";
			return MODEACTION_DENY;	
		}
		else
		{
			// We're taking the mode off
			if (el)
			{
				for (modelist::iterator it = el->begin(); it != el->end(); it++)
				{
					if(parameter == it->mask)
					{
						el->erase(it);
						if(el->size() == 0)
						{
							channel->Shrink(infokey);
							delete el;
						}
						return MODEACTION_ALLOW;
					}
					else
					{
						/* Tried to remove something that wasn't set */
						TellNotSet(source, channel, parameter);
					}
				}
				parameter = "";
				return MODEACTION_DENY;
			}
			else
			{
				// Hmm, taking an exception off a non-existant list, DIE
				parameter = "";
				return MODEACTION_DENY;
			}
		}
		return MODEACTION_DENY;
	}

	virtual std::string& GetInfoKey()
	{
		return infokey;
	}

	virtual void DoChannelDelete(chanrec* chan)
	{
		modelist* list;
		chan->GetExt(infokey, list);

		if (list)
		{
			chan->Shrink(infokey);
			delete list;
		}
	}

	virtual void DoSyncChannel(chanrec* chan, Module* proto, void* opaque)
	{
		modelist* list;
		chan->GetExt(infokey, list);
		if (list)
		{
			for (modelist::iterator it = list->begin(); it != list->end(); it++)
			{
				proto->ProtoSendMode(opaque, TYPE_CHANNEL, chan, "+" + std::string(1, mode) + " " + it->mask);
			}
		}
	}

	virtual void DoCleanup(int target_type, void* item)
	{
	}
	
	virtual bool ValidateParam(userrec* source, chanrec* channel, std::string &parameter)
	{
		return true;
	}
	
	virtual bool TellListTooLong(userrec* source, chanrec* channel, std::string &parameter)
	{
		return false;
	}
	
	virtual void TellAlreadyOnList(userrec* source, chanrec* channel, std::string &parameter)
	{
	}
	
	virtual void TellNotSet(userrec* source, chanrec* channel, std::string &parameter)
	{
		
	}
};

#endif
