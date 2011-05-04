/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef INSPIRCD_LISTMODE_PROVIDER
#define INSPIRCD_LISTMODE_PROVIDER

/** The number of items a listmode's list may contain
 */
class ListLimit
{
public:
	std::string mask;
	unsigned int limit;
};

/** Max items per channel by name
 */
typedef std::list<ListLimit> limitlist;

class CoreExport ListExtItem : public LocalExtItem
{
 public:
	ListExtItem(const std::string& Key, Module* parent) : LocalExtItem(EXTENSIBLE_CHANNEL, Key, parent)
	{
	}

	virtual ~ListExtItem()
	{
	}

	inline modelist* get(const Extensible* container) const
	{
		return static_cast<modelist*>(get_raw(container));
	}

	inline modelist* make(Extensible* container)
	{
		modelist* ml = new modelist;
		modelist* old = static_cast<modelist*>(set_raw(container, static_cast<void*>(ml)));
		this->free(old);
		return ml;
	}

	inline void unset(Extensible* container)
	{
		modelist* old = static_cast<modelist*>(unset_raw(container));
		this->free(old);
	}

	virtual void free(void* item);
};


/** The base class for list modes, should be inherited.
 */
class CoreExport ListModeBase : public ModeHandler
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
	ListExtItem extItem;

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
		configtag(ctag), extItem(name + "_mode_list", Creator)
	{
		list = true;
	}

	inline void init()
	{
		DoRehash();
		ServerInstance->Extensions.Register(&extItem);
	}

	virtual void DisplayList(User* user, Channel* channel);
	virtual void PopulateChanModes(Channel* channel, irc::modestacker& stack);
	virtual const modelist* GetList(Channel* channel);

	virtual void DisplayEmptyList(User* user, Channel* channel)
	{
		user->WriteNumeric(endoflistnumeric, "%s %s :%s", user->nick.c_str(), channel->name.c_str(), endofliststring.c_str());
	}

	virtual void RemoveMode(Channel* channel, irc::modestacker* stack);
	virtual void RemoveMode(User*, irc::modestacker* stack);

	/** Perform a rehash of this mode's configuration data
	 */
	virtual void DoRehash();

	/** Handle the list mode.
	 * See mode.h
	 */
	virtual ModeAction OnModeChange(User* source, User*, Channel* channel, std::string &parameter, bool adding);

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
