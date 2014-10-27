/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef INSPIRCD_LISTMODE_PROVIDER
#define INSPIRCD_LISTMODE_PROVIDER

/** Get the time as a string
 */
inline std::string stringtime()
{
	std::ostringstream TIME;
	TIME << ServerInstance->Time();
	return TIME.str();
}

/** An item in a listmode's list
 */
class ListItem
{
public:
	std::string nick;
	std::string mask;
	std::string time;
};

/** The number of items a listmode's list may contain
 */
class ListLimit
{
public:
	std::string mask;
	unsigned int limit;
};

/** Items stored in the channel's list
 */
typedef std::list<ListItem> modelist;
/** Max items per channel by name
 */
typedef std::list<ListLimit> limitlist;

/** The base class for list modes, should be inherited.
 */
class ListModeBase : public ModeHandler
{
 protected:
	/** Numeric to use when outputting the list
	 */
	unsigned int listnumeric;
	/** Numeric to indicate end of list
	 */
	unsigned int endoflistnumeric;
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
	/** Storage key
	 */
	SimpleExtItem<modelist> extItem;

	/** Constructor.
	 * @param Instance The creator of this class
	 * @param modechar Mode character
	 * @param eolstr End of list string
	 * @pram lnum List numeric
	 * @param eolnum End of list numeric
	 * @param autotidy Automatically tidy list entries on add
	 * @param ctag Configuration tag to get limits from
	 */
	ListModeBase(Module* Creator, const std::string& Name, char modechar, const std::string &eolstr, unsigned int lnum, unsigned int eolnum, bool autotidy, const std::string &ctag = "banlist")
		: ModeHandler(Creator, Name, modechar, PARAM_ALWAYS, MODETYPE_CHANNEL), 
		listnumeric(lnum), endoflistnumeric(eolnum), endofliststring(eolstr), tidy(autotidy),
		configtag(ctag), extItem("listbase_mode_" + name + "_list", Creator)
	{
		list = true;
	}

	/** See mode.h
	 */
	std::pair<bool,std::string> ModeSet(User*, User*, Channel* channel, const std::string &parameter)
	{
		modelist* el = extItem.get(channel);
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
	virtual void DisplayList(User* user, Channel* channel)
	{
		modelist* el = extItem.get(channel);
		if (el)
		{
			for (modelist::reverse_iterator it = el->rbegin(); it != el->rend(); ++it)
			{
				user->WriteNumeric(listnumeric, "%s %s %s %s %s", user->nick.c_str(), channel->name.c_str(), it->mask.c_str(), (it->nick.length() ? it->nick.c_str() : ServerInstance->Config->ServerName.c_str()), it->time.c_str());
			}
		}
		user->WriteNumeric(endoflistnumeric, "%s %s :%s", user->nick.c_str(), channel->name.c_str(), endofliststring.c_str());
	}

	virtual void DisplayEmptyList(User* user, Channel* channel)
	{
		user->WriteNumeric(endoflistnumeric, "%s %s :%s", user->nick.c_str(), channel->name.c_str(), endofliststring.c_str());
	}

	/** Remove all instances of the mode from a channel.
	 * See mode.h
	 * @param channel The channel to remove all instances of the mode from
	 */
	virtual void RemoveMode(Channel* channel, irc::modestacker* stack)
	{
		modelist* el = extItem.get(channel);
		if (el)
		{
			irc::modestacker modestack(false);

			for (modelist::iterator it = el->begin(); it != el->end(); it++)
			{
				if (stack)
					stack->Push(this->GetModeChar(), it->mask);
				else
					modestack.Push(this->GetModeChar(), it->mask);
			}

			if (stack)
				return;

			std::vector<std::string> stackresult;
			stackresult.push_back(channel->name);
			while (modestack.GetStackedLine(stackresult))
			{
				ServerInstance->SendMode(stackresult, ServerInstance->FakeClient);
				stackresult.clear();
				stackresult.push_back(channel->name);
			}
		}
	}

	/** See mode.h
	 */
	virtual void RemoveMode(User*, irc::modestacker* stack)
	{
		/* Listmodes dont get set on users */
	}

	/** Perform a rehash of this mode's configuration data
	 */
	virtual void DoRehash()
	{
		ConfigTagList tags = ServerInstance->Config->ConfTags(configtag);

		chanlimits.clear();

		for (ConfigIter i = tags.first; i != tags.second; i++)
		{
			// For each <banlist> tag
			ConfigTag* c = i->second;
			ListLimit limit;
			limit.mask = c->getString("chan");
			limit.limit = c->getInt("limit");

			if (limit.mask.size() && limit.limit > 0)
				chanlimits.push_back(limit);
		}

		// Add the default entry. This is inserted last so if the user specifies a
		// wildcard record in the config it will take precedence over this entry.
		ListLimit limit;
		limit.mask = "*";
		limit.limit = 64;
		chanlimits.push_back(limit);
	}

	/** Populate the Implements list with the correct events for a List Mode
	 */
	virtual void DoImplements(Module* m)
	{
		ServerInstance->Modules->AddService(extItem);
		this->DoRehash();
		Implementation eventlist[] = { I_OnSyncChannel, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, m, sizeof(eventlist)/sizeof(Implementation));
	}

	/** Handle the list mode.
	 * See mode.h
	 */
	virtual ModeAction OnModeChange(User* source, User*, Channel* channel, std::string &parameter, bool adding)
	{
		// Try and grab the list
		modelist* el = extItem.get(channel);

		if (adding)
		{
			if (tidy)
				ModeParser::CleanMask(parameter);

			if (parameter.length() > 250)
				return MODEACTION_DENY;

			// If there was no list
			if (!el)
			{
				// Make one
				el = new modelist;
				extItem.set(channel, el);
			}

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
				if (InspIRCd::Match(channel->name, it->mask))
				{
					// We have a pattern matching the channel...
					maxsize = el->size();
					if (!IS_LOCAL(source) || (maxsize < it->limit))
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
					else
						break;
				}
			}

			/* List is full, give subclass a chance to send a custom message */
			if (!TellListTooLong(source, channel, parameter))
			{
				source->WriteNumeric(478, "%s %s %s :Channel ban/ignore list is full", source->nick.c_str(), channel->name.c_str(), parameter.c_str());
			}

			parameter.clear();
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
						if (el->empty())
						{
							extItem.unset(channel);
						}
						return MODEACTION_ALLOW;
					}
				}
				/* Tried to remove something that wasn't set */
				TellNotSet(source, channel, parameter);
				parameter.clear();
				return MODEACTION_DENY;
			}
			else
			{
				/* Hmm, taking an exception off a non-existant list, DIE */
				TellNotSet(source, channel, parameter);
				parameter.clear();
				return MODEACTION_DENY;
			}
		}
		return MODEACTION_DENY;
	}

	/** Syncronize channel item list with another server.
	 * See modules.h
	 * @param chan Channel to syncronize
	 * @param proto Protocol module pointer
	 * @param opaque Opaque connection handle
	 */
	virtual void DoSyncChannel(Channel* chan, Module* proto, void* opaque)
	{
		modelist* mlist = extItem.get(chan);
		irc::modestacker modestack(true);
		std::vector<std::string> stackresult;
		std::vector<TranslateType> types;
		types.push_back(TR_TEXT);
		if (mlist)
		{
			for (modelist::iterator it = mlist->begin(); it != mlist->end(); it++)
			{
				modestack.Push(std::string(1, mode)[0], it->mask);
			}
		}
		while (modestack.GetStackedLine(stackresult))
		{
			types.assign(stackresult.size(), this->GetTranslateType());
			proto->ProtoSendMode(opaque, TYPE_CHANNEL, chan, stackresult, types);
			stackresult.clear();
		}
	}

	/** Clean up module on unload
	 * @param target_type Type of target to clean
	 * @param item Item to clean
	 */
	virtual void DoCleanup(int, void*)
	{
	}

	/** Validate parameters.
	 * Overridden by implementing module.
	 * @param source Source user adding the parameter
	 * @param channel Channel the parameter is being added to
	 * @param parameter The actual parameter being added
	 * @return true if the parameter is valid
	 */
	virtual bool ValidateParam(User*, Channel*, std::string&)
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
	virtual bool TellListTooLong(User*, Channel*, std::string&)
	{
		return false;
	}

	/** Tell the user an item is already on the list.
	 * Overridden by implementing module.
	 * @param source Source user adding the parameter
	 * @param channel Channel the parameter is being added to
	 * @param parameter The actual parameter being added
	 */
	virtual void TellAlreadyOnList(User*, Channel*, std::string&)
	{
	}

	/** Tell the user that the parameter is not in the list.
	 * Overridden by implementing module.
	 * @param source Source user removing the parameter
	 * @param channel Channel the parameter is being removed from
	 * @param parameter The actual parameter being removed
	 */
	virtual void TellNotSet(User*, Channel*, std::string&)
	{
	}
};

#endif
