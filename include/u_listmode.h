/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

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

/* Updated to use the <banlist> config tag if it exists
 * Written by Om <omster@gmail.com>, December 2005.
 * Based on code previously written by Om - April 2005
 * Updated to new API July 8th 2006 by Brain
 * Originally based on m_chanprotect and m_silence
 */

/** Get the time as a string
 */
inline std::string stringtime()
{
	std::ostringstream TIME;
	TIME << time(NULL); 
	return TIME.str();
}

/** An item in a listmode's list
 */
class ListItem : public classbase
{
public:
	std::string nick;
	irc::string mask;
	std::string time;
};

/** The number of items a listmode's list may contain
 */
class ListLimit : public classbase
{
public:
	std::string mask;
	unsigned int limit;
};

/** Items stored in the channel's list
 */
typedef std::vector<ListItem> modelist;
/** Max items per channel by name
 */
typedef std::vector<ListLimit> limitlist;

/** A request used to check if a user is on a channel's list or not
 */
class ListModeRequest : public Request
{
 public:
	userrec* user;
	chanrec* chan;

	/** Check if a user is on a channel's list.
	 * The Event::Send() event returns true if the user is on the channel's list.
	 * @param sender Sending module
	 * @param target Target module
	 * @param u User to check against
	 * @param c Channel to check against
	 */
	ListModeRequest(Module* sender, Module* target, userrec* u, chanrec* c) : Request(sender, target, "LM_CHECKLIST"), user(u), chan(c)
	{
	}

	/** Destructor
	 */
	~ListModeRequest()
	{
	}
};

/** The base class for list modes, should be inherited.
 */
class ListModeBase : public ModeHandler
{
 protected:
	/** Storage key
	 */
	std::string infokey;
	/** Numeric to use when outputting the list
	 */
	std::string listnumeric;
	/** Numeric to indicate end of list
	 */
	std::string endoflistnumeric;
	/** String to send for end of list
	 */
	std::string endofliststring;
	/** Automatically tidy up entries
	 */
	bool tidy;
	/** Config tag to check for max items per channel
	 */
 	std::string configtag;
	/** Limits on a per-channel basis read from the tag
	 * specified in ListModeBase::configtag
	 */
	limitlist chanlimits;
 
 public:
	/** Constructor.
	 * @param Instance The creator of this class
	 * @param modechar Mode character
	 * @param eolstr End of list string
	 * @pram lnum List numeric
	 * @param eolnum End of list numeric
	 * @param autotidy Automatically tidy list entries on add
	 * @param ctag Configuration tag to get limits from
	 */
	ListModeBase(InspIRCd* Instance, char modechar, const std::string &eolstr, const std::string &lnum, const std::string &eolnum, bool autotidy, const std::string &ctag = "banlist")
 	: ModeHandler(Instance, modechar, 1, 1, true, MODETYPE_CHANNEL, false), listnumeric(lnum), endoflistnumeric(eolnum), endofliststring(eolstr), tidy(autotidy), configtag(ctag)
	{
		this->DoRehash();
		infokey = "listbase_mode_" + std::string(1, mode) + "_list";
	}

	/** See mode.h 
	 */
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

	/** Display the list for this mode
	 * @param user The user to send the list to
	 * @param channel The channel the user is requesting the list for
	 */
	virtual void DisplayList(userrec* user, chanrec* channel)
	{
		modelist* el;
		channel->GetExt(infokey, el);
		if (el)
		{
			for (modelist::reverse_iterator it = el->rbegin(); it != el->rend(); ++it)
			{
				user->WriteServ("%s %s %s %s %s %s", listnumeric.c_str(), user->nick, channel->name, it->mask.c_str(), it->nick.c_str(), it->time.c_str());
			}
		}
		user->WriteServ("%s %s %s :%s", endoflistnumeric.c_str(), user->nick, channel->name, endofliststring.c_str());
	}

	/** Remove all instances of the mode from a channel.
	 * See mode.h
	 * @param channel The channel to remove all instances of the mode from
	 */
	virtual void RemoveMode(chanrec* channel)
	{
		modelist* el;
		channel->GetExt(infokey, el);
		if (el)
		{
			irc::modestacker modestack(false);
			std::deque<std::string> stackresult;
			const char* mode_junk[MAXMODES+2];
			mode_junk[0] = channel->name;
			userrec* n = new userrec(ServerInstance);
			n->SetFd(FD_MAGIC_NUMBER);
			for (modelist::iterator it = el->begin(); it != el->end(); it++)
			{
				modestack.Push(this->GetModeChar(), assign(it->mask));
			}
			while (modestack.GetStackedLine(stackresult))
			{
				for (size_t j = 0; j < stackresult.size(); j++)
				{
					mode_junk[j+1] = stackresult[j].c_str();
				}
				ServerInstance->SendMode(mode_junk, stackresult.size() + 1, n);		
			}

			delete n;
		}
	}

	/** See mode.h
	 */
	virtual void RemoveMode(userrec* user)
	{
		/* Listmodes dont get set on users */
	}

	/** Perform a rehash of this mode's configuration data
	 */
	virtual void DoRehash()
	{
		ConfigReader Conf(ServerInstance);

		chanlimits.clear();

		for (int i = 0; i < Conf.Enumerate(configtag); i++)
		{
			// For each <banlist> tag
			ListLimit limit;
			limit.mask = Conf.ReadValue(configtag, "chan", i);
			limit.limit = Conf.ReadInteger(configtag, "limit", i, true);

			if (limit.mask.size() && limit.limit > 0)
				chanlimits.push_back(limit);
		}
		if (chanlimits.size() == 0)
		{
			ListLimit limit;
			limit.mask = "*";
			limit.limit = 64;
			chanlimits.push_back(limit);
		}
	}

	/** Populate the Implements list with the correct events for a List Mode
	 */
	virtual void DoImplements(char* List)
	{
		List[I_OnChannelDelete] = List[I_OnSyncChannel] = List[I_OnCleanup] = List[I_OnRehash] = 1;
	}

	/** Handle the list mode.
	 * See mode.h
	 */
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
				if (parameter == it->mask)
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
						if (ValidateParam(source, channel, parameter))
						{
							// And now add the mask onto the list...
							ListItem e;
							e.mask = assign(parameter);
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
			if (!TellListTooLong(source, channel, parameter))
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
					if (parameter == it->mask)
					{
						el->erase(it);
						if (el->size() == 0)
						{
							channel->Shrink(infokey);
							delete el;
						}
						return MODEACTION_ALLOW;
					}
				}
				/* Tried to remove something that wasn't set */
				TellNotSet(source, channel, parameter);
				parameter = "";
				return MODEACTION_DENY;
			}
			else
			{
				/* Hmm, taking an exception off a non-existant list, DIE */
				TellNotSet(source, channel, parameter);
				parameter = "";
				return MODEACTION_DENY;
			}
		}
		return MODEACTION_DENY;
	}

	/** Get Extensible key for this mode
	 */
	virtual std::string& GetInfoKey()
	{
		return infokey;
	}

	/** Handle channel deletion.
	 * See modules.h.
	 * @param chan Channel being deleted
	 */
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

	/** Syncronize channel item list with another server.
	 * See modules.h
	 * @param chan Channel to syncronize
	 * @param proto Protocol module pointer
	 * @param opaque Opaque connection handle
	 */
	virtual void DoSyncChannel(chanrec* chan, Module* proto, void* opaque)
	{
		modelist* list;
		chan->GetExt(infokey, list);
		irc::modestacker modestack(true);
		std::deque<std::string> stackresult;
		if (list)
		{
			for (modelist::iterator it = list->begin(); it != list->end(); it++)
			{
				modestack.Push(std::string(1, mode)[0], assign(it->mask));
			}
		}
		while (modestack.GetStackedLine(stackresult))
		{
			irc::stringjoiner mode_join(" ", stackresult, 0, stackresult.size() - 1);
			std::string line = mode_join.GetJoined();
			proto->ProtoSendMode(opaque, TYPE_CHANNEL, chan, line);
		}
	}

	/** Clean up module on unload
	 * @param target_type Type of target to clean
	 * @param item Item to clean
	 */
	virtual void DoCleanup(int target_type, void* item)
	{
	}
	
	/** Validate parameters.
	 * Overridden by implementing module.
	 * @param source Source user adding the parameter
	 * @param channel Channel the parameter is being added to
	 * @param parameter The actual parameter being added
	 * @return true if the parameter is valid
	 */
	virtual bool ValidateParam(userrec* source, chanrec* channel, std::string &parameter)
	{
		return true;
	}
	
	/** Tell the user the list is too long.
	 * Overridden by implementing module.
	 * @param source Source user adding the parameter
	 * @param channel Channel the parameter is being added to
	 * @param parameter The actual parameter being added
	 * @return Ignored
	 */
	virtual bool TellListTooLong(userrec* source, chanrec* channel, std::string &parameter)
	{
		return false;
	}
	
	/** Tell the user an item is already on the list.
	 * Overridden by implementing module.
	 * @param source Source user adding the parameter
	 * @param channel Channel the parameter is being added to
	 * @param parameter The actual parameter being added
	 */
	virtual void TellAlreadyOnList(userrec* source, chanrec* channel, std::string &parameter)
	{
	}
	
	/** Tell the user that the parameter is not in the list.
	 * Overridden by implementing module.
	 * @param source Source user removing the parameter
	 * @param channel Channel the parameter is being removed from
	 * @param parameter The actual parameter being removed
	 */
	virtual void TellNotSet(userrec* source, chanrec* channel, std::string &parameter)
	{
	}
};

#endif

