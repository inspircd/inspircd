/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012-2013, 2017-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@gmail.com>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2008 Craig Edwards <brain@inspircd.org>
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


#pragma once

#include "ctables.h"
#include "modechange.h"

/**
 * Holds the values for different type of modes
 * that can exist, USER or CHANNEL type.
 */
enum ModeType
{
	/** User mode */
	MODETYPE_USER = 0,
	/** Channel mode */
	MODETYPE_CHANNEL = 1
};

/**
 * These fixed values can be used to proportionally compare module-defined prefixes to known values.
 * For example, if your module queries a Channel, and is told that user 'joebloggs' has the prefix
 * '$', and you dont know what $ means, then you can compare it to these three values to determine
 * its worth against them. For example if '$' had a value of 15000, you would know it is of higher
 * status than voice, but lower status than halfop.
 * No two modes should have equal prefix values.
 */
enum PrefixModeValue
{
	/* +v */
	VOICE_VALUE	=	10000,
	/* +h */
	HALFOP_VALUE	=	20000,
	/* +o */
	OP_VALUE	=	30000
};

enum ParamSpec
{
	/** No parameters */
	PARAM_NONE,
	/** Parameter required on mode setting only */
	PARAM_SETONLY,
	/** Parameter always required */
	PARAM_ALWAYS
};

class PrefixMode;
class ListModeBase;
class ParamModeBase;

/** Each mode is implemented by ONE ModeHandler class.
 * You must derive ModeHandler and add the child class to
 * the list of modes handled by the ircd, using
 * ModeParser::AddMode. When the mode you implement is
 * set by a user, the virtual function OnModeChange is
 * called. If you specify a value greater than 0 for
 * parameters_on or parameters_off, then when the mode is
 * set or unset respectively, std::string& parameter will
 * contain the parameter given by the user, else it will
 * contain an empty string. You may alter this parameter
 * string, and if you alter it to an empty string, and your
 * mode is expected to have a parameter, then this is
 * equivalent to returning false.
 */
class CoreExport ModeHandler
	: public ServiceProvider
{
public:
	/** The underlying type of a mode rank. */
	typedef unsigned long Rank;

	/** The underlying type of a mode id. */
	typedef size_t Id;

	enum Class
	{
		MC_PREFIX,
		MC_LIST,
		MC_PARAM,
		MC_OTHER
	};

private:
	/** The opaque id of this mode assigned by the mode parser
	 */
	Id modeid;

protected:
	/** What kind of parameters does the mode take?
	 */
	ParamSpec parameters_taken;

	/**
	 * The mode letter you're implementing.
	 */
	char mode;

	/**
	 * True if the mode requires oper status
	 * to set.
	 */
	bool oper = false;

	/**
	 * Mode is a 'list' mode. The behaviour
	 * of your mode is now set entirely within
	 * the class as of the 1.1 api, rather than
	 * inside the mode parser as in the 1.0 api,
	 * so the only use of this value (along with
	 * IsListMode()) is for the core to determine
	 * whether your module can produce 'lists' or not
	 * (e.g. banlists, etc)
	 */
	bool list = false;

	/**
	 * The mode type, either MODETYPE_USER or
	 * MODETYPE_CHANNEL.
	 */
	ModeType m_type;

	/** The object type of this mode handler
	 */
	const Class type_id;

	/** The prefix rank required to set this mode on channels. */
	Rank ranktoset = HALFOP_VALUE;

	/** The prefix rank required to unset this mode on channels. */
	Rank ranktounset = HALFOP_VALUE;

	/** If non-empty then the syntax of the parameter for this mode. */
	std::string syntax;

public:
	/**
	 * The constructor for ModeHandler initializes the mode handler.
	 * The constructor of any class you derive from ModeHandler should
	 * probably call this constructor with the parameters set correctly.
	 * @param me The module which created this mode
	 * @param name A one-word name for the mode
	 * @param modeletter The mode letter you wish to handle
	 * @param params Parameters taken by the mode
	 * @param type Type of the mode (MODETYPE_USER or MODETYPE_CHANNEL)
	 * @param mclass The object type of this mode handler, one of ModeHandler::Class
	 */
	ModeHandler(Module* me, const std::string& name, char modeletter, ParamSpec params, ModeType type, Class mclass = MC_OTHER);
	Cullable::Result Cull() override;

	/** Register this object in the ModeParser
	 */
	void RegisterService() override;

	/**
	 * Returns true if the mode is a list mode
	 */
	bool IsListMode() const { return list; }

	/**
	 * Check whether this mode is a prefix mode
	 * @return non-NULL if this mode is a prefix mode, NULL otherwise
	 */
	PrefixMode* IsPrefixMode();

	/**
	 * Check whether this mode is a prefix mode
	 * @return non-NULL if this mode is a prefix mode, NULL otherwise
	 */
	const PrefixMode* IsPrefixMode() const;

	/**
	 * Check whether this mode handler inherits from ListModeBase
	 * @return non-NULL if this mode handler inherits from ListModeBase, NULL otherwise
	 */
	ListModeBase* IsListModeBase();

	/**
	 * Check whether this mode handler inherits from ListModeBase
	 * @return non-NULL if this mode handler inherits from ListModeBase, NULL otherwise
	 */
	const ListModeBase* IsListModeBase() const;

	/**
	 * Check whether this mode handler inherits from ParamModeBase
	 * @return non-NULL if this mode handler inherits from ParamModeBase, NULL otherwise
	 */
	ParamModeBase* IsParameterMode();

	/**
	 * Check whether this mode handler inherits from ParamModeBase
	 * @return non-NULL if this mode handler inherits from ParamModeBase, NULL otherwise
	 */
	const ParamModeBase* IsParameterMode() const;

	/**
	 * Returns the mode's type
	 */
	inline ModeType GetModeType() const { return m_type; }
	/**
	 * Returns true if the mode can only be set/unset by an oper
	 */
	inline bool NeedsOper() const { return oper; }
	/**
	 * Check if the mode needs a parameter for adding or removing
	 * @param adding True to check if the mode needs a parameter when setting, false to check if the mode needs a parameter when unsetting
	 * @return True if the mode needs a parameter for the specified action, false if it doesn't
	 */
	bool NeedsParam(bool adding) const;
	/**
	 * Returns the mode character this handler handles.
	 * @return The mode character
	 */
	char GetModeChar() const { return mode; }

	/** Return the id of this mode which is used in User::modes and
	 * Channel::modes as the index to determine whether a mode is set.
	 */
	Id GetId() const { return modeid; }

	/** For user modes, return the current parameter, if any
	 */
	virtual std::string GetUserParameter(const User* user) const;

	/**
	 * Called when a channel mode change access check for your mode occurs.
	 * @param source Contains the user setting the mode.
	 * @param channel The destination channel the modes are being set on.
	 * @param change Information regarding the mode change.
	 */
	virtual ModResult AccessCheck(User* source, Channel* channel, Modes::Change& change);

	/**
	 * Called when a mode change for your mode occurs.
	 * @param source Contains the user setting the mode.
	 * @param dest For usermodes, contains the destination user the mode is being set on. For channelmodes, this is an undefined value.
	 * @param channel For channel modes, contains the destination channel the modes are being set on. For usermodes, this is an undefined value.
	 * @param change Information regarding the mode change.
	 * @return true to allow the mode, or false to prevent the mode, also see the description of 'parameter'.
	 */
	virtual bool OnModeChange(User* source, User* dest, Channel* channel, Modes::Change& change);

	/**
	 * If your mode is a listmode, then this method will be called for displaying an item list, e.g. on MODE \#channel +modechar
	 * without any parameter or other modes in the command.
	 * @param user The user issuing the command
	 * @param channel The channel they're requesting an item list of (e.g. a banlist, or an exception list etc)
	 */
	virtual void DisplayList(User* user, Channel* channel);

	/** In the event that the mode should be given a parameter, and no parameter was provided, this method is called.
	 * This allows you to give special information to the user, or handle this any way you like.
	 * @param user The user issuing the mode change
	 * @param dest For user mode changes, the target of the mode. For channel mode changes, NULL.
	 * @param channel For channel mode changes, the target of the mode. For user mode changes, NULL.
	 */
	virtual void OnParameterMissing(User* user, User* dest, Channel* channel);

	/** Called when a user attempts to set a mode and the parameter is invalid.
	 * @param user The user issuing the mode change
	 * @param targetchannel Either the channel target or NULL if changing a user mode.
	 * @param targetuser Either the user target or NULL if changing a channel mode.
	 * @param parameter The invalid parameter.
	 */
	virtual void OnParameterInvalid(User* user, Channel* targetchannel, User* targetuser, const std::string& parameter);

	/**
	 * If your mode is a listmode, this method will be called to display an empty list (just the end of list numeric)
	 * @param user The user issuing the command
	 * @param channel The channel they're requesting an item list of (e.g. a banlist, or an exception list etc)
	 */
	virtual void DisplayEmptyList(User* user, Channel* channel);

	/**
	 * If your mode needs special action during a server sync to determine which side wins when comparing timestamps,
	 * override this function and use it to return true or false. The default implementation just returns true if
	 * theirs < ours. This will only be called for non-listmodes with parameters, when adding the mode and where
	 * theirs == ours (therefore the default implementation will always return false).
	 * @param their_param Their parameter if the mode has a parameter
	 * @param our_param Our parameter if the mode has a parameter
	 * @param channel The channel we are checking against
	 * @return True if the other side wins the merge, false if we win the merge for this mode.
	 */
	virtual bool ResolveModeConflict(const std::string& their_param, const std::string& our_param, Channel* channel);

	/**
	 * When a MODETYPE_USER mode handler is being removed, the core will call this method for every user on the server.
	 * The usermode will be removed using the appropriate server mode using InspIRCd::SendMode().
	 * @param user The user which the server wants to remove your mode from
	 */
	void RemoveMode(User* user);

	/**
	 * When a MODETYPE_CHANNEL mode handler is being removed, the server will call this method for every channel on the server.
	 * The mode handler has to populate the given modestacker with mode changes that remove the mode from the channel.
	 * The default implementation of this method can remove all kinds of channel modes except listmodes.
	 * In the case of listmodes, the entire list of items must be added to the modestacker (which is handled by ListModeBase,
	 * so if you inherit from it or your mode can be removed by the default implementation then you do not have to implement
	 * this function).
	 * @param channel The channel which the server wants to remove your mode from
	 * @param changelist Mode change list to populate with the removal of this mode
	 */
	virtual void RemoveMode(Channel* channel, Modes::ChangeList& changelist);

	/** Retrieves the level required to modify this mode.
	 * @param adding Whether the mode is being added or removed.
	 */
	inline Rank GetLevelRequired(bool adding) const
	{
		return adding ? ranktoset : ranktounset;
	}

	/** Retrieves the syntax of the parameter for this mode. */
	const std::string& GetSyntax() const { return syntax; }

	friend class ModeParser;
};

/**
 * Prefix modes are channel modes that grant a specific rank to members having prefix mode set.
 * They require a parameter when setting and unsetting; the parameter is always a member of the channel.
 * A prefix mode may be set on any number of members on a channel, but for a given member a given prefix
 * mode is either set or not set, in other words members cannot have the same prefix mode set more than once.
 *
 * A rank of a member is defined as the rank given by the 'strongest' prefix mode that member has.
 * Other parts of the IRCd use this rank to determine whether a channel action is allowable for a user or not.
 * The rank of a prefix mode is constant, i.e. the same rank value is given to all users having that prefix mode set.
 *
 * Note that it is possible that the same action requires a different rank on a different channel;
 * for example changing the topic on a channel having +t set requires a rank that is >= than the rank of a halfop,
 * but there is no such restriction when +t isn't set.
 */
class CoreExport PrefixMode
	: public ModeHandler
{
protected:
	/** The prefix character granted by this mode. '@' for op, '+' for voice, etc.
	 * If 0, this mode does not have a visible prefix character.
	 */
	char prefix;

	/** The prefix rank of this mode, used to compare prefix
	 * modes
	 */
	Rank prefixrank;

	/** Whether a client with this prefix can remove it from themself. */
	bool selfremove = true;

public:
	/** Sorts a container of PrefixMode* objects descending by their rank. */
	struct Sorter final
	{
		bool operator()(const PrefixMode* lhs, const PrefixMode* rhs) const
		{
			return lhs->GetPrefixRank() > rhs->GetPrefixRank();
		}
	};

	/**
	 * Constructor
	 * @param Creator The module creating this mode
	 * @param Name The user-friendly one word name of the prefix mode, e.g.: "op", "voice"
	 * @param ModeLetter The mode letter of this mode
	 * @param PrefixRank Rank given by this prefix mode, see explanation above
	 * @param PrefixChar Prefix character, or 0 if the mode has no prefix character
	 */
	PrefixMode(Module* Creator, const std::string& Name, char ModeLetter, Rank PrefixRank = 0, char PrefixChar = 0);

	/** @copydoc ModeHandler::AccessCheck */
	ModResult AccessCheck(User* source, Channel* channel, Modes::Change& change) override;

	/** @copydoc ModeHandler::OnModeChange */
	bool OnModeChange(User* source, User* dest, Channel* channel, Modes::Change& change) override;

	/**
	 * Updates the configuration of this prefix.
	 * @param rank The prefix rank of this mode.
	 * @param setrank The prefix rank required to set this mode on channels.
	 * @param unsetrank The prefix rank required to set this unmode on channels.
	 * @param selfrm Whether a client with this prefix can remove it from themself.
	 */
	void Update(Rank rank, Rank setrank, Rank unsetrank, bool selfrm);

	/** @copydoc ModeHandler::RemoveMode(Channel*,Modes::ChangeList&) */
	void RemoveMode(Channel* channel, Modes::ChangeList& changelist) override;

	/**
	* Determines whether a user with this prefix mode can remove it.
	*/
	bool CanSelfRemove() const { return selfremove; }

	/**
	 * Mode prefix or 0. If this is defined, you should
	 * also implement GetPrefixRank() to return an integer
	 * value for this mode prefix.
	 */
	char GetPrefix() const { return prefix; }

	/**
	 * Get the 'value' of this modes prefix.
	 * determines which to display when there are multiple.
	 * The mode with the highest value is ranked first. See the
	 * PrefixModeValue enum and Channel::GetPrefixValue() for
	 * more information.
	 */
	Rank GetPrefixRank() const { return prefixrank; }
};

/** A prebuilt mode handler which handles a simple user mode, e.g. no parameters, usable by any user, with no extra
 * behaviour to the mode beyond the basic setting and unsetting of the mode, not allowing the mode to be set if it
 * is already set and not allowing it to be unset if it is already unset.
 * An example of a simple user mode is user mode +w.
 */
class CoreExport SimpleUserMode
	: public ModeHandler
{
public:
	SimpleUserMode(Module* Creator, const std::string& Name, char modeletter, bool operonly = false)
		: ModeHandler(Creator, Name, modeletter, PARAM_NONE, MODETYPE_USER)
	{
		oper = operonly;
	}

	/** @copydoc ModeHandler::OnModeChange */
	bool OnModeChange(User* source, User* dest, Channel* channel, Modes::Change& change) override;
};

/** A prebuilt mode handler which handles a simple channel mode, e.g. no parameters, usable by any user, with no extra
 * behaviour to the mode beyond the basic setting and unsetting of the mode, not allowing the mode to be set if it
 * is already set and not allowing it to be unset if it is already unset.
 * An example of a simple channel mode is channel mode +s.
 */
class CoreExport SimpleChannelMode
	: public ModeHandler
{
public:

	SimpleChannelMode(Module* Creator, const std::string& Name, char modeletter, bool operonly = false)
		: ModeHandler(Creator, Name, modeletter, PARAM_NONE, MODETYPE_CHANNEL)
	{
		oper = operonly;
	}

	/** @copydoc ModeHandler::OnModeChange */
	bool OnModeChange(User* source, User* dest, Channel* channel, Modes::Change& change) override;
};

/**
 * The ModeWatcher class can be used to alter the behaviour of a mode implemented
 * by the core or by another module. To use ModeWatcher, derive a class from it,
 * and attach it to the mode using Server::AddModeWatcher and Server::DelModeWatcher.
 * A ModeWatcher will be called both before and after the mode change.
 */
class CoreExport ModeWatcher
	: public Cullable
{
private:
	/**
	 * The mode name this class is watching
	 */
	const std::string mode;

	/**
	 * The mode type being watched (user or channel)
	 */
	ModeType m_type;

public:
	ModuleRef creator;

	/**
	 * The constructor initializes the mode and the mode type
	 */
	ModeWatcher(Module* creator, const std::string& modename, ModeType type);

	/**
	 * The default destructor does nothing.
	 */
	~ModeWatcher() override;

	/**
	 * Get the mode name being watched
	 * @return The mode name being watched
	 */
	const std::string& GetModeName() const { return mode; }

	/**
	 * Get the mode type being watched
	 * @return The mode type being watched (user or channel)
	 */
	ModeType GetModeType() const { return m_type; }

	/**
	 * Before the mode character is processed by its handler, this method will be called.
	 * @param source The sender of the mode
	 * @param dest The target user for the mode, if you are watching a user mode
	 * @param channel The target channel for the mode, if you are watching a channel mode
	 * @param change Information regarding the mode change.
	 * If you alter the parameter you are given, the mode handler will see your altered version
	 * when it handles the mode.
	 * @return True to allow the mode change to go ahead, false to abort it. If you abort the
	 * change, the mode handler (and ModeWatcher::AfterMode()) will never see the mode change.
	 */
	virtual bool BeforeMode(User* source, User* dest, Channel* channel, Modes::Change& change);

	/**
	 * After the mode character has been processed by the ModeHandler, this method will be called.
	 * @param source The sender of the mode
	 * @param dest The target user for the mode, if you are watching a user mode
	 * @param channel The target channel for the mode, if you are watching a channel mode
	 * @param change Information regarding the mode change.
	 */
	virtual void AfterMode(User* source, User* dest, Channel* channel, const Modes::Change& change);
};

/** The mode parser handles routing of modes and handling of mode strings.
 * It marshalls, controls and maintains both ModeWatcher and ModeHandler classes,
 * parses client to server MODE strings for user and channel modes, and performs
 * processing for the 004 mode list numeric, amongst other things.
 */
class CoreExport ModeParser final
{
public:
	/** The maximum number of modes which can be created. */
	static constexpr ModeHandler::Id MODEID_MAX = 64;

	/** The maximum length of a mode parameter. */
	static constexpr size_t MODE_PARAM_MAX = 250;

	/** Type of the container that maps mode names to ModeHandlers
	 */
	typedef std::unordered_map<std::string, ModeHandler*, irc::insensitive, irc::StrHashComp> ModeHandlerMap;

	/** Type of the container that contains whether a mode is set. */
	typedef std::bitset<MODEID_MAX> ModeStatus;

private:
	/** Type of the container that maps mode names to ModeWatchers
	 */
	typedef insp::flat_multimap<std::string, ModeWatcher*> ModeWatcherMap;

	/** Last item in the ModeType enum
	 */
	static constexpr unsigned int MODETYPE_LAST = 2;

	/** Mode handlers for each mode indexed by the mode index
	 */
	ModeHandler* modehandlers[MODETYPE_LAST][MODEID_MAX];

	/** An array of mode handlers indexed by the mode id
	 */
	ModeHandler* modehandlersbyid[MODETYPE_LAST][MODEID_MAX];

	/** A map of mode handlers keyed by their name
	 */
	ModeHandlerMap modehandlersbyname[MODETYPE_LAST];

	/** Lists of mode handlers by type
	 */
	struct
	{
		/** List of mode handlers that inherit from ListModeBase
		 */
		std::vector<ListModeBase*> list;

		/** List of mode handlers that inherit from PrefixMode
		 */
		std::vector<PrefixMode*> prefix;
	} mhlist;

	/** Mode watcher classes
	 */
	ModeWatcherMap modewatchermap;

	/** Last processed mode change
	 */
	Modes::ChangeList LastChangeList;

	/** Attempts to apply a mode change to a user or channel
	 * @param user The user who triggered the mode change.
	 * @param usertarget If non-NULL then the user to change the modes of.
	 * @param chantarget If non-NULL then the channel to change the modes of.
	 * @param mcitem The actual mode change to attempt.
	 * @param skipacl Whether to skip access checks for the mode change.
	 */
	bool TryMode(User* user, User* usertarget, Channel* chantarget, Modes::Change& mcitem, bool skipacl);

	/** Allocates an unused id for the given mode type, throws a ModuleException if out of ids.
	 * @param mh The mode to allocate the id for
	 * @return The id
	 */
	ModeHandler::Id AllocateModeId(ModeHandler* mh);

public:
	typedef std::vector<ListModeBase*> ListModeList;
	typedef std::vector<PrefixMode*> PrefixModeList;

	typedef unsigned int ModeProcessFlag;
	enum ModeProcessFlags
	{
		/** If only this flag is specified, the mode change will be global
		 * and parameter modes will have their parameters explicitly set
		 * (not merged). This is the default.
		 */
		MODE_NONE = 0,

		/** If this flag is set then the parameters of non-listmodes will be
		 * merged according to their conflict resolution rules.
		 * Does not affect user modes, channel modes without a parameter and
		 * listmodes.
		 */
		MODE_MERGE = 1,

		/** If this flag is set then the linking module will ignore the mode change
		 * and not send it to other servers. The mode change will be processed
		 * locally and sent to local user(s) as usual.
		 */
		MODE_LOCALONLY = 2,

		/** If this flag is set then the mode change will be subject to access checks.
		 * For more information see the documentation of the PrefixMode class,
		 * ModeHandler::ranktoset and ModeHandler::AccessCheck().
		 * Modules may explicitly allow a mode change regardless of this flag by returning
		 * MOD_RES_ALLOW from the OnPreMode hook. Only affects channel mode changes.
		 */
		MODE_CHECKACCESS = 4
	};

	ModeParser();

	/** Determines whether the specified character is a valid mode. */
	static bool IsModeChar(char chr);

	/** Retrieves the index of the specified mode char within a mode bitset. */
	static size_t GetModeIndex(char chr);

	/** Tidy a banmask. This makes a banmask 'acceptable' if fields are left out.
	 * E.g.
	 *
	 * nick -> nick!*@*
	 *
	 * nick!user -> nick!user@*
	 *
	 * host.name -> *!*\@host.name
	 *
	 * user\@host.name -> *!user\@host.name
	 *
	 * This method can be used on both IPV4 and IPV6 user masks.
	 */
	static void CleanMask(std::string& mask);

	/** Gets the last mode change to be processed. */
	const Modes::ChangeList& GetLastChangeList() const { return LastChangeList; }

	/** Add a mode to the mode parser.
	 * Throws a ModuleException if the mode cannot be added.
	 */
	void AddMode(ModeHandler* mh);

	/** Delete a mode from the mode parser.
	 * When a mode is deleted, the mode handler will be called
	 * for every user (if it is a user mode) or for every  channel
	 * (if it is a channel mode) to unset the mode on all objects.
	 * This prevents modes staying in the system which no longer exist.
	 * @param mh The mode handler to remove
	 * @return True if the mode was successfully removed.
	 */
	bool DelMode(ModeHandler* mh);

	/** Add a mode watcher.
	 * A mode watcher is triggered before and after a mode handler is
	 * triggered. See the documentation of class ModeWatcher for more
	 * information.
	 * @param mw The ModeWatcher you want to add
	 */
	void AddModeWatcher(ModeWatcher* mw);

	/** Delete a mode watcher.
	 * A mode watcher is triggered before and after a mode handler is
	 * triggered. See the documentation of class ModeWatcher for more
	 * information.
	 * @param mw The ModeWatcher you want to delete
	 * @return True if the ModeWatcher was deleted correctly
	 */
	bool DelModeWatcher(ModeWatcher* mw);

	/** Process a list of mode changes entirely. If the mode changes do not fit into one MODE line
	 * then multiple MODE lines are generated.
	 * @param user The source of the mode change, can be a server user.
	 * @param targetchannel Channel to apply the mode change on. NULL if changing modes on a channel.
	 * @param targetuser User to apply the mode change on. NULL if changing modes on a user.
	 * @param changelist Modes to change in form of a Modes::ChangeList.
	 * @param flags Optional flags controlling how the mode change is processed,
	 * defaults to MODE_NONE.
	 */
	void Process(User* user, Channel* targetchannel, User* targetuser, Modes::ChangeList& changelist, ModeProcessFlag flags = MODE_NONE);

	/** Process a single MODE line's worth of mode changes, taking max modes and line length limits
	 * into consideration. Return value indicates how many modes were processed.
	 * @param user The source of the mode change, can be a server user.
	 * @param targetchannel Channel to apply the mode change on. NULL if changing modes on a channel.
	 * @param targetuser User to apply the mode change on. NULL if changing modes on a user.
	 * @param changelist Modes to change in form of a Modes::ChangeList. May not process
	 * the entire list due to MODE line length and max modes limitations.
	 * @param flags Optional flags controlling how the mode change is processed,
	 * defaults to MODE_NONE.
	 * @param beginindex Index of the first element in changelist to process. Mode changes before
	 * the element with this index are ignored.
	 * @return Number of mode changes processed from changelist.
	 */
	size_t ProcessSingle(User* user, Channel* targetchannel, User* targetuser, Modes::ChangeList& changelist, ModeProcessFlag flags = MODE_NONE, size_t beginindex = 0);

	/** Turn a list of parameters compatible with the format of the MODE command into
	 * Modes::ChangeList form. All modes are processed, regardless of max modes. Unknown modes
	 * are skipped.
	 * @param user The source of the mode change, can be a server user. Error numerics are sent to
	 * this user.
	 * @param type MODETYPE_USER if this is a user mode change or MODETYPE_CHANNEL if this
	 * is a channel mode change.
	 * @param parameters List of strings describing the mode change to convert to a ChangeList.
	 * Must be using the same format as the parameters of a MODE command.
	 * @param changelist ChangeList object to populate.
	 * @param beginindex Index of the first element that is part of the MODE list in the parameters
	 * container. Defaults to 1.
	 * @param endindex Index of the first element that is not part of the MODE list. By default,
	 * the entire container is considered part of the MODE list.
	 */
	void ModeParamsToChangeList(User* user, ModeType type, const std::vector<std::string>& parameters, Modes::ChangeList& changelist, size_t beginindex = 1, size_t endindex = SIZE_MAX);

	/** Find the mode handler for a given mode name and type.
	 * @param modename The mode name to search for.
	 * @param mt Type of mode to search for, user or channel.
	 * @return A pointer to a ModeHandler class, or NULL of there isn't a handler for the given mode name.
	 */
	ModeHandler* FindMode(const std::string& modename, ModeType mt);

	/** Find the mode handler for a given mode and type.
	 * @param modeletter mode letter to search for
	 * @param mt type of mode to search for, user or channel
	 * @returns a pointer to a ModeHandler class, or NULL of there isn't a handler for the given mode
	 */
	ModeHandler* FindMode(unsigned char modeletter, ModeType mt);

	/** Find the mode handler for the given prefix mode
	 * @param modeletter The mode letter to search for
	 * @return A pointer to the PrefixMode or NULL if the mode wasn't found or it isn't a prefix mode
	 */
	PrefixMode* FindPrefixMode(unsigned char modeletter);

	/** Find the mode handler for the prefix mode nearest the given rank.
	 * @param rank The rank to search for prefix modes near.
	 * @return A pointer to the PrefixMode or NULL if a prefix mode wasn't found.
	 */
	PrefixMode* FindNearestPrefixMode(ModeHandler::Rank rank) const;

	/** Find a mode handler by its prefix.
	 * If there is no mode handler with the given prefix, NULL will be returned.
	 * @param pfxletter The prefix to find, e.g. '@'
	 * @return The mode handler which handles this prefix, or NULL if there is none.
	 */
	PrefixMode* FindPrefix(unsigned char pfxletter) const;

	/** Get a list of all mode handlers that inherit from ListModeBase
	 * @return A list containing ListModeBase modes
	 */
	const ListModeList& GetListModes() const { return mhlist.list; }

	/** Get a list of all prefix modes
	 * @return A list containing all prefix modes
	 */
	const PrefixModeList& GetPrefixModes() const { return mhlist.prefix; }

	/** Get a mode name -> ModeHandler* map containing all modes of the given type
	 * @param mt Type of modes to return, MODETYPE_USER or MODETYPE_CHANNEL
	 * @return A map of mode handlers of the given type
	 */
	const ModeHandlerMap& GetModes(ModeType mt) const { return modehandlersbyname[mt]; }

	/** Show the list of a list mode to a user. Modules can deny the listing.
	 * @param user User to show the list to.
	 * @param chan Channel to show the list of.
	 * @param mh List mode to show the list of.
	 */
	void ShowListModeList(User* user, Channel* chan, ModeHandler* mh);
};

inline PrefixMode* ModeHandler::IsPrefixMode()
{
	return (this->type_id == MC_PREFIX ? static_cast<PrefixMode*>(this) : nullptr);
}

inline const PrefixMode* ModeHandler::IsPrefixMode() const
{
	return (this->type_id == MC_PREFIX ? static_cast<const PrefixMode*>(this) : nullptr);
}

inline ListModeBase* ModeHandler::IsListModeBase()
{
	return (this->type_id == MC_LIST ? reinterpret_cast<ListModeBase*>(this) : nullptr);
}

inline const ListModeBase* ModeHandler::IsListModeBase() const
{
	return (this->type_id == MC_LIST ? reinterpret_cast<const ListModeBase*>(this) : nullptr);
}

inline ParamModeBase* ModeHandler::IsParameterMode()
{
	return (this->type_id == MC_PARAM ? reinterpret_cast<ParamModeBase*>(this) : nullptr);
}

inline const ParamModeBase* ModeHandler::IsParameterMode() const
{
	return (this->type_id == MC_PARAM ? reinterpret_cast<const ParamModeBase*>(this) : nullptr);
}
