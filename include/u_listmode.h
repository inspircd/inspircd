/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef INSPIRCD_LISTMODE_PROVIDER
#define INSPIRCD_LISTMODE_PROVIDER

/** Get the time as a string
 */
inline std::string stringtime(InspIRCd* Instance)
{
	std::ostringstream TIME;
	TIME << Instance->Time();
	return TIME.str();
}

/** An item in a listmode's list
 */
class ListItem : public classbase
{
public:
	std::string nick;
	std::string mask;
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
typedef std::list<ListItem> modelist;
/** Max items per channel by name
 */
typedef std::list<ListLimit> limitlist;

/** A request used to check if a user is on a channel's list or not
 */
class ListModeRequest : public Request
{
 public:
	User* user;
	std::string literal;
	const char extban;
	Channel* chan;

	/** Check if a user is on a channel's list.
	 * The Event::Send() event returns the ban string if the user is on the channel's list,
	 * or NULL if the user is not on the list.
	 * @param sender Sending module
	 * @param target Target module
	 * @param u User to check against
	 * @param c Channel to check against
	 */
	ListModeRequest(Module* sender, Module* target, User* u, Channel* c) : Request(sender, target, "LM_CHECKLIST"), user(u), literal(""), extban(0), chan(c)
	{
	}

	/** Check if a literal string is on a channel's list, optionally using an extban char.
	 * The Event::Send() event returns the ban string if the user is on the channel's list,
	 * or NULL if the user is not on the list.
	 * @param sender Sending module
	 * @param target Target module
	 * @param literalstr String to check against, e.g. "Bob!Bobbertson@weeblshouse"
	 * @param extbanchar Extended ban character to use for the match, or a null char if not using extban
	 */
	ListModeRequest(Module* sender, Module* target, std::string literalstr, char extbanchar, Channel* channel) : Request(sender, target, "LM_CHECKLIST_EX"), user(NULL), literal(literalstr), extban(extbanchar), chan(channel)
	{
	}

	/** Check if a literal string is on a channel's list, optionally using an extban char.
	 * The Event::Send() event returns the ban string if the user is on the channel's list,
	 * or NULL if the user is not on the list.
	 * @param sender Sending module
	 * @param target Target module
	 * @param User to check against, e.g. "Bob!Bobbertson@weeblshouse"
	 * @param extbanchar Extended ban character to use for the match, or a null char if not using extban
	 */
	ListModeRequest(Module* sender, Module* target, User* u, char extbanchar, Channel* channel) : Request(sender, target, "LM_CHECKLIST_EX"), user(u), literal(""), extban(extbanchar), chan(channel)
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
	/** Constructor.
	 * @param Instance The creator of this class
	 * @param modechar Mode character
	 * @param eolstr End of list string
	 * @pram lnum List numeric
	 * @param eolnum End of list numeric
	 * @param autotidy Automatically tidy list entries on add
	 * @param ctag Configuration tag to get limits from
	 */
	ListModeBase(InspIRCd* Instance, Module* Creator, char modechar, const std::string &eolstr, unsigned int lnum, unsigned int eolnum, bool autotidy, const std::string &ctag = "banlist")
		: ModeHandler(Instance, Creator, modechar, 1, 1, true, MODETYPE_CHANNEL, false), listnumeric(lnum), endoflistnumeric(eolnum), endofliststring(eolstr), tidy(autotidy), configtag(ctag)
	{
		this->DoRehash();
		infokey = "listbase_mode_" + std::string(1, mode) + "_list";
	}

	/** See mode.h
	 */
	std::pair<bool,std::string> ModeSet(User*, User*, Channel* channel, const std::string &parameter)
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
	virtual void DisplayList(User* user, Channel* channel)
	{
		modelist* el;
		channel->GetExt(infokey, el);
		if (el)
		{
			for (modelist::reverse_iterator it = el->rbegin(); it != el->rend(); ++it)
			{
				user->WriteNumeric(listnumeric, "%s %s %s %s %s", user->nick.c_str(), channel->name.c_str(), it->mask.c_str(), (it->nick.length() ? it->nick.c_str() : ServerInstance->Config->ServerName), it->time.c_str());
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
		modelist* el;
		channel->GetExt(infokey, el);
		if (el)
		{
			irc::modestacker modestack(ServerInstance, false);

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
	virtual void DoImplements(Module* m)
	{
		Implementation eventlist[] = { I_OnChannelDelete, I_OnSyncChannel, I_OnCleanup, I_OnRehash, I_OnRequest };
		ServerInstance->Modules->Attach(eventlist, m, 5);
	}

	/** Handle the list mode.
	 * See mode.h
	 */
	virtual ModeAction OnModeChange(User* source, User*, Channel* channel, std::string &parameter, bool adding)
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
				if (InspIRCd::Match(channel->name, it->mask))
				{
					// We have a pattern matching the channel...
					maxsize = el->size();
					if (IS_LOCAL(source) || (maxsize < it->limit))
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
							e.time = stringtime(ServerInstance);

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
				source->WriteNumeric(478, "%s %s %s :Channel ban/ignore list is full", source->nick.c_str(), channel->name.c_str(), parameter.c_str());
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
	virtual void DoChannelDelete(Channel* chan)
	{
		modelist* mlist;
		chan->GetExt(infokey, mlist);

		if (mlist)
		{
			chan->Shrink(infokey);
			delete mlist;
		}
	}

	/** Syncronize channel item list with another server.
	 * See modules.h
	 * @param chan Channel to syncronize
	 * @param proto Protocol module pointer
	 * @param opaque Opaque connection handle
	 */
	virtual void DoSyncChannel(Channel* chan, Module* proto, void* opaque)
	{
		modelist* mlist;
		chan->GetExt(infokey, mlist);
		irc::modestacker modestack(ServerInstance, true);
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

	virtual const char* DoOnRequest(Request* request)
	{
		ListModeRequest* LM = (ListModeRequest*)request;
		if (strcmp("LM_CHECKLIST", request->GetId()) == 0)
		{
			modelist* mlist;
			LM->chan->GetExt(GetInfoKey(), mlist);
			if (mlist)
			{
				std::string mask = LM->user->nick + "!" + LM->user->ident + "@" + LM->user->GetIPString();
				for (modelist::iterator it = mlist->begin(); it != mlist->end(); ++it)
				{
					if (InspIRCd::Match(LM->user->GetFullRealHost(), it->mask) || InspIRCd::Match(LM->user->GetFullHost(), it->mask) || (InspIRCd::MatchCIDR(mask, it->mask)))
						return it->mask.c_str();
				}
				return NULL;
			}
		}
		else if (strcmp("LM_CHECKLIST_EX", request->GetId()) == 0)
		{
			modelist* mlist;
			LM->chan->GetExt(GetInfoKey(), mlist);

			if (mlist)
			{
				if (LM->user)
				{
					LM->literal = LM->user->nick + "!" + LM->user->ident + "@" + LM->user->GetIPString();
				}

				for (modelist::iterator it = mlist->begin(); it != mlist->end(); it++)
				{
					if (LM->extban && it->mask.length() > 1 && it->mask[0] == LM->extban && it->mask[1] == ':')
					{
						std::string ext = it->mask.substr(2);
						if (LM->user)
						{
							if (InspIRCd::Match(LM->user->GetFullRealHost(), ext) || InspIRCd::Match(LM->user->GetFullHost(), ext) || (InspIRCd::MatchCIDR(LM->literal, ext)))
							{
								return it->mask.c_str();
							}
						}
						else if (InspIRCd::Match(LM->literal, ext))
						{
							return it->mask.c_str();
						}
					}
					else
					{
						if (LM->user)
						{
							if (InspIRCd::Match(LM->user->GetFullRealHost(), it->mask) || InspIRCd::Match(LM->user->GetFullHost(), it->mask) || (InspIRCd::MatchCIDR(LM->literal, it->mask)))
							{
								return it->mask.c_str();
							}
						}
						else if (InspIRCd::Match(LM->literal, it->mask))
						{
							return it->mask.c_str();
						}
					}
				}
			}
		}
		return NULL;
	}

};

#endif
