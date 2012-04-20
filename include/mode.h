/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2004-2006, 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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


#ifndef __MODE_H
#define __MODE_H

/* Forward declarations. */
class User;

#include "ctables.h"
#include "channels.h"

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
 * Holds mode actions - modes can be allowed or denied.
 */
enum ModeAction
{
	MODEACTION_DENY = 0, /* Drop the mode change, AND a parameter if its a parameterized mode */
	MODEACTION_ALLOW = 1 /* Allow the mode */
};

/**
 * Used to mask off the mode types in the mode handler
 * array. Used in a simple two instruction hashing function
 * "(modeletter - 65) OR mask"
 */
enum ModeMasks
{
	MASK_USER = 128,	/* A user mode */
	MASK_CHANNEL = 0	/* A channel mode */
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

/**
 * Used by ModeHandler::ModeSet() to return the state of a mode upon a channel or user.
 * The pair contains an activity flag, true if the mode is set with the given parameter,
 * and the parameter of the mode (or the parameter provided) in the std::string.
 */
typedef std::pair<bool,std::string> ModePair;

/** Each mode is implemented by ONE ModeHandler class.
 * You must derive ModeHandler and add the child class to
 * the list of modes handled by the ircd, using
 * ModeParser::AddMode. When the mode you implement is
 * set by a user, the virtual function OnModeChange is
 * called. If you specify a value greater than 0 for
 * parameters_on or parameters_off, then when the mode is
 * set or unset respectively, std::string &parameter will
 * contain the parameter given by the user, else it will
 * contain an empty string. You may alter this parameter
 * string, and if you alter it to an empty string, and your
 * mode is expected to have a parameter, then this is
 * equivalent to returning MODEACTION_DENY.
 */
class CoreExport ModeHandler : public Extensible
{
 protected:
	/**
	 * Creator/owner pointer
	 */
	InspIRCd* ServerInstance;
	/**
	 * The mode letter you're implementing.
	 */
	char mode;
	/**
	 * Number of parameters when being set
	 */
	int n_params_on;
	/**
	 * Number of parameters when being unset
	 */
	int n_params_off;
	/**
	 * Mode is a 'list' mode. The behaviour
	 * of your mode is now set entirely within
	 * the class as of the 1.1 api, rather than
	 * inside the mode parser as in the 1.0 api,
	 * so the only use of this value (along with
	 * IsListMode()) is for the core to determine
	 * wether your module can produce 'lists' or not
	 * (e.g. banlists, etc)
	 */
	bool list;
	/**
	 * The mode type, either MODETYPE_USER or
	 * MODETYPE_CHANNEL.
	 */
	ModeType m_type;
	/**
	 * The mode parameter translation type
	 */
	TranslateType m_paramtype;
	/**
	 * True if the mode requires oper status
	 * to set.
	 */
	bool oper;

	/** Mode prefix, or 0
	 */
	char prefix;

	/** Number of items with this mode set on them
	 */
	unsigned int count;

	/** The prefix char needed on channel to use this mode,
	 * only checked for channel modes
	 */
	char prefixneeded;

 public:
	/**
	 * The constructor for ModeHandler initalizes the mode handler.
	 * The constructor of any class you derive from ModeHandler should
	 * probably call this constructor with the parameters set correctly.
	 * @param modeletter The mode letter you wish to handle
	 * @param parameters_on The number of parameters your mode takes when being set. Note that any nonzero value is treated as 1.
	 * @param parameters_off The number of parameters your mode takes when being unset. Note that any nonzero value is treated as 1.
	 * @param listmode Set to true if your mode is a listmode, e.g. it will respond to MODE #channel +modechar with a list of items
	 * @param ModeType Set this to MODETYPE_USER for a usermode, or MODETYPE_CHANNEL for a channelmode.
	 * @param operonly Set this to true if only opers should be allowed to set or unset the mode.
	 * @param mprefix For listmodes where parameters are NICKNAMES which are on the channel (for example, +ohv), you may define a prefix.
	 * When you define a prefix, it can be returned in NAMES, WHO etc if it has the highest value (as returned by GetPrefixRank())
	 * In the core, the only modes to implement prefixes are +ovh (ops, voice, halfop) which define the prefix characters @, % and +
	 * and the rank values OP_VALUE, HALFOP_VALUE and VOICE_VALUE respectively. Any prefixes you define should have unique values proportional
	 * to these three defaults or proportional to another mode in a module you depend on. See src/cmode_o.cpp as an example.
	 */
	ModeHandler(InspIRCd* Instance, char modeletter, int parameters_on, int parameters_off, bool listmode, ModeType type, bool operonly,
		char mprefix = 0, char prefixrequired = '%', TranslateType translate = TR_TEXT);
	/**
	 * The default destructor does nothing
	 */
	virtual ~ModeHandler();
	/**
	 * Returns true if the mode is a list mode
	 */
	bool IsListMode();
	/**
	 * Mode prefix or 0. If this is defined, you should
	 * also implement GetPrefixRank() to return an integer
	 * value for this mode prefix.
	 */
	char GetPrefix();
	/** Get number of items with this mode set on them
	 */
	virtual unsigned int GetCount();
	/** Adjust usage count returned by GetCount
	 */
	virtual void ChangeCount(int modifier);
	/**
	 * Get the 'value' of this modes prefix.
	 * determines which to display when there are multiple.
	 * The mode with the highest value is ranked first. See the
	 * PrefixModeValue enum and Channel::GetPrefixValue() for
	 * more information.
	 */
	virtual unsigned int GetPrefixRank();
	/**
	 * Returns the mode's type
	 */
	ModeType GetModeType();
	/**
	 * Returns the mode's parameter translation type
	 */
	TranslateType GetTranslateType();
	/**
	 * Returns true if the mode can only be set/unset by an oper
	 */
	bool NeedsOper();
	/**
	 * Returns the number of parameters for the mode. Any non-zero
	 * value should be considered to be equivalent to one.
	 * @param adding If this is true, the number of parameters required to set the mode should be returned, otherwise the number of parameters required to unset the mode shall be returned.
	 * @return The number of parameters the mode expects
	 */
	int GetNumParams(bool adding);
	/**
	 * Returns the mode character this handler handles.
	 * @return The mode character
	 */
	char GetModeChar();

	/** For user modes, return the current parameter, if any
	 */
	virtual std::string GetUserParameter(User* useor);

	/**
	 * Called when a mode change for your mode occurs.
	 * @param source Contains the user setting the mode.
	 * @param dest For usermodes, contains the destination user the mode is being set on. For channelmodes, this is an undefined value.
	 * @param channel For channel modes, contains the destination channel the modes are being set on. For usermodes, this is an undefined value.
	 * @param parameter The parameter for your mode, if you indicated that your mode requires a parameter when being set or unset. Note that
	 * if you alter this value, the new value becomes the one displayed and send out to the network, also, if you set this to an empty string
	 * but you specified your mode REQUIRES a parameter, this is equivalent to returning MODEACTION_DENY and will prevent the mode from being
	 * displayed.
	 * @param adding This value is true when the mode is being set, or false when it is being unset.
	 * @return MODEACTION_ALLOW to allow the mode, or MODEACTION_DENY to prevent the mode, also see the description of 'parameter'.
	 */
	virtual ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool servermode = false); /* Can change the mode parameter as its a ref */
	/**
	 * If your mode is a listmode, then this method will be called for displaying an item list, e.g. on MODE #channel +modechar
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

	/**
	 * If your mode is a listmode, this method will be called to display an empty list (just the end of list numeric)
	 * @param user The user issuing the command
	 * @param channel The channel tehy're requesting an item list of (e.g. a banlist, or an exception list etc)
	 */
	virtual void DisplayEmptyList(User* user, Channel* channel);

	/**
	 * If your mode needs special action during a server sync to determine which side wins when comparing timestamps,
	 * override this function and use it to return true or false. The default implementation just returns true if
	 * theirs < ours. This will only be called for non-listmodes with parameters, when adding the mode and where
	 * theirs == ours (therefore the default implementation will always return false).
	 * @param theirs The timestamp of the remote side
	 * @param ours The timestamp of the local side
	 * @param their_param Their parameter if the mode has a parameter
	 * @param our_param Our parameter if the mode has a parameter
	 * @param channel The channel we are checking against
	 * @return True if the other side wins the merge, false if we win the merge for this mode.
	 */
	virtual bool CheckTimeStamp(time_t theirs, time_t ours, const std::string &their_param, const std::string &our_param, Channel* channel);

	/**
	 * When a remote server needs to bounce a set of modes, it will call this method for every mode
	 * in the mode string to determine if the mode is set or not.
	 * @param source of the mode change, this will be NULL for a server mode
	 * @param dest Target user of the mode change, if this is a user mode
	 * @param channel Target channel of the mode change, if this is a channel mode
	 * @param parameter The parameter given for the mode change, or an empty string
	 * @returns The first value of the pair should be true if the mode is set with the given parameter.
	 * In the case of permissions modes such as channelmode +o, this should return true if the user given
	 * as the parameter has the given privilage on the given channel. The string value of the pair will hold
	 * the current setting for this mode set locally, when the bool is true, or, the parameter given.
	 * This allows the local server to enforce our locally set parameters back to a remote server.
	 */
	virtual ModePair ModeSet(User* source, User* dest, Channel* channel, const std::string &parameter);

	/**
	 * When a MODETYPE_USER mode handler is being removed, the server will call this method for every user on the server.
	 * Your mode handler should remove its user mode from the user by sending the appropriate server modes using
	 * InspIRCd::SendMode(). The default implementation of this method can remove simple modes which have no parameters,
	 * and can be used when your mode is of this type, otherwise you must implement a more advanced version of it to remove
	 * your mode properly from each user.
	 * @param user The user which the server wants to remove your mode from
	 */
	virtual void RemoveMode(User* user, irc::modestacker* stack = NULL);

	/**
	 * When a MODETYPE_CHANNEL mode handler is being removed, the server will call this method for every channel on the server.
	 * Your mode handler should remove its user mode from the channel by sending the appropriate server modes using
	 * InspIRCd::SendMode(). The default implementation of this method can remove simple modes which have no parameters,
	 * and can be used when your mode is of this type, otherwise you must implement a more advanced version of it to remove
	 * your mode properly from each channel. Note that in the case of listmodes, you should remove the entire list of items.
	 * @param channel The channel which the server wants to remove your mode from
	 */
	virtual void RemoveMode(Channel* channel, irc::modestacker* stack = NULL);

	char GetNeededPrefix();

	void SetNeededPrefix(char needsprefix);
};

/** A prebuilt mode handler which handles a simple user mode, e.g. no parameters, usable by any user, with no extra
 * behaviour to the mode beyond the basic setting and unsetting of the mode, not allowing the mode to be set if it
 * is already set and not allowing it to be unset if it is already unset.
 * An example of a simple user mode is user mode +w.
 */
class CoreExport SimpleUserModeHandler : public ModeHandler
{
 public:
	SimpleUserModeHandler(InspIRCd* Instance, char modeletter);
	virtual ~SimpleUserModeHandler();
	virtual ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool servermode = false);
};

/** A prebuilt mode handler which handles a simple channel mode, e.g. no parameters, usable by any user, with no extra
 * behaviour to the mode beyond the basic setting and unsetting of the mode, not allowing the mode to be set if it
 * is already set and not allowing it to be unset if it is already unset.
 * An example of a simple channel mode is channel mode +s.
 */
class CoreExport SimpleChannelModeHandler : public ModeHandler
{
 public:
	SimpleChannelModeHandler(InspIRCd* Instance, char modeletter);
	virtual ~SimpleChannelModeHandler();
	virtual ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool servermode = false);
};

/**
 * The ModeWatcher class can be used to alter the behaviour of a mode implemented
 * by the core or by another module. To use ModeWatcher, derive a class from it,
 * and attach it to the mode using Server::AddModeWatcher and Server::DelModeWatcher.
 * A ModeWatcher will be called both before and after the mode change.
 */
class CoreExport ModeWatcher : public Extensible
{
 protected:
	/**
	 * Creator/owner pointer
	 */
	InspIRCd* ServerInstance;
	/**
	 * The mode letter this class is watching
	 */
	char mode;
	/**
	 * The mode type being watched (user or  channel)
	 */
	ModeType m_type;

 public:
	/**
	 * The constructor initializes the mode and the mode type
	 */
	ModeWatcher(InspIRCd* Instance, char modeletter, ModeType type);
	/**
	 * The default destructor does nothing.
	 */
	virtual ~ModeWatcher();

	/**
	 * Get the mode character being watched
	 * @return The mode character being watched
	 */
	char GetModeChar();
	/**
	 * Get the mode type being watched
	 * @return The mode type being watched (user or channel)
	 */
	ModeType GetModeType();

	/**
	 * Before the mode character is processed by its handler, this method will be called.
	 * @param source The sender of the mode
	 * @param dest The target user for the mode, if you are watching a user mode
	 * @param channel The target channel for the mode, if you are watching a channel mode
	 * @param parameter The parameter of the mode, if the mode is supposed to have a parameter.
	 * If you alter the parameter you are given, the mode handler will see your atered version
	 * when it handles the mode.
	 * @param adding True if the mode is being added and false if it is being removed
	 * @type The mode type, either MODETYPE_USER or MODETYPE_CHANNEL
	 * @return True to allow the mode change to go ahead, false to abort it. If you abort the
	 * change, the mode handler (and ModeWatcher::AfterMode()) will never see the mode change.
	 */
	virtual bool BeforeMode(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, ModeType type, bool servermode = false);
	/**
	 * After the mode character has been processed by the ModeHandler, this method will be called.
	 * @param source The sender of the mode
	 * @param dest The target user for the mode, if you are watching a user mode
	 * @param channel The target channel for the mode, if you are watching a channel mode
	 * @param parameter The parameter of the mode, if the mode is supposed to have a parameter.
	 * You cannot alter the parameter here, as the mode handler has already processed it.
	 * @param adding True if the mode is being added and false if it is being removed
	 * @type The mode type, either MODETYPE_USER or MODETYPE_CHANNEL
	 */
	virtual void AfterMode(User* source, User* dest, Channel* channel, const std::string &parameter, bool adding, ModeType type, bool servermode = false);
};

typedef std::vector<ModeWatcher*>::iterator ModeWatchIter;

/** The mode parser handles routing of modes and handling of mode strings.
 * It marshalls, controls and maintains both ModeWatcher and ModeHandler classes,
 * parses client to server MODE strings for user and channel modes, and performs
 * processing for the 004 mode list numeric, amongst other things.
 */
class CoreExport ModeParser : public classbase
{
 private:
	/**
	 * Creator/owner pointer
	 */
	InspIRCd* ServerInstance;
	/** Mode handlers for each mode, to access a handler subtract
	 * 65 from the ascii value of the mode letter.
	 * The upper bit of the value indicates if its a usermode
	 * or a channel mode, so we have 256 of them not 64.
	 */
	ModeHandler* modehandlers[256];
	/** Mode watcher classes arranged in the same way as the
	 * mode handlers, except for instead of having 256 of them
	 * we have 256 lists of them.
	 */
	std::vector<ModeWatcher*> modewatchers[256];
	/** Displays the current modes of a channel or user.
	 * Used by ModeParser::Process.
	 */
	void DisplayCurrentModes(User *user, User* targetuser, Channel* targetchannel, const char* text);

	/** The string representing the last set of modes to be parsed.
	 * Use GetLastParse() to get this value, to be used for  display purposes.
	 */
	std::string LastParse;
	std::deque<std::string> LastParseParams;
	std::deque<TranslateType> LastParseTranslate;

	unsigned int sent[256];

	unsigned int seq;

 public:

	/** The constructor initializes all the RFC basic modes by using ModeParserAddMode().
	 */
	ModeParser(InspIRCd* Instance);

	/** Used to check if user 'd' should be allowed to do operation 'MASK' on channel 'chan'.
	 * for example, should 'user A' be able to 'op' on 'channel B'.
	 */
	User* SanityChecks(User *user,const char *dest,Channel *chan,int status);
	/** Grant a built in privilage (e.g. ops, halfops, voice) to a user on a channel
	 */
	const char* Grant(User *d,Channel *chan,int MASK);
	/** Revoke a built in privilage (e.g. ops, halfops, voice) to a user on a channel
	 */
	const char* Revoke(User *d,Channel *chan,int MASK);
	/** Tidy a banmask. This makes a banmask 'acceptable' if fields are left out.
	 * E.g.
	 *
	 * nick -> nick!*@*
	 *
	 * nick!ident -> nick!ident@*
	 *
	 * host.name -> *!*@host.name
	 *
	 * ident@host.name -> *!ident@host.name
	 *
	 * This method can be used on both IPV4 and IPV6 user masks.
	 */
	static void CleanMask(std::string &mask);
	/** Get the last string to be processed, as it was sent to the user or channel.
	 * Use this to display a string you just sent to be parsed, as the actual output
	 * may be different to what you sent after it has been 'cleaned up' by the parser.
	 * @return Last parsed string, as seen by users.
	 */
	const std::string& GetLastParse();
	const std::deque<std::string>& GetLastParseParams() { return LastParseParams; }
	const std::deque<TranslateType>& GetLastParseTranslate() { return LastParseTranslate; }
	/** Add a mode to the mode parser.
	 * @return True if the mode was successfully added.
	 */
	bool AddMode(ModeHandler* mh);
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
	 * @return True if the ModeWatcher was added correctly
	 */
	bool AddModeWatcher(ModeWatcher* mw);
	/** Delete a mode watcher.
	 * A mode watcher is triggered before and after a mode handler is
	 * triggered. See the documentation of class ModeWatcher for more
	 * information.
	 * @param mw The ModeWatcher you want to delete
	 * @return True if the ModeWatcher was deleted correctly
	 */
	bool DelModeWatcher(ModeWatcher* mw);
	/** Process a set of mode changes from a server or user.
	 * @param parameters The parameters of the mode change, in the format
	 * they would be from a MODE command.
	 * @param user The user setting or removing the modes. When the modes are set
	 * by a server, an 'uninitialized' User is used, where *user::nick == NULL
	 * and *user->server == NULL.
	 * @param servermode True if a server is setting the mode.
	 */
	void Process(const std::vector<std::string>& parameters, User *user, bool servermode);

	/** Find the mode handler for a given mode and type.
	 * @param modeletter mode letter to search for
	 * @param type of mode to search for, user or channel
	 * @returns a pointer to a ModeHandler class, or NULL of there isnt a handler for the given mode
	 */
	ModeHandler* FindMode(unsigned const char modeletter, ModeType mt);

	/** Find a mode handler by its prefix.
	 * If there is no mode handler with the given prefix, NULL will be returned.
	 * @param pfxletter The prefix to find, e.g. '@'
	 * @return The mode handler which handles this prefix, or NULL if there is none.
	 */
	ModeHandler* FindPrefix(unsigned const char pfxletter);

	/** Returns a list of mode characters which are usermodes.
	 * This is used in the 004 numeric when users connect.
	 */
	std::string UserModeList();

	/** Returns a list of channel mode characters which are listmodes.
	 * This is used in the 004 numeric when users connect.
	 */
	std::string ChannelModeList();

	/** Returns a list of channel mode characters which take parameters.
	 * This is used in the 004 numeric when users connect.
	 */
	std::string ParaModeList();

	/** Generates a list of modes, comma seperated by type:
	 *  1; Listmodes EXCEPT those with a prefix
	 *  2; Modes that take a param when adding or removing
	 *  3; Modes that only take a param when adding
	 *  4; Modes that dont take a param
	 */
	std::string GiveModeList(ModeMasks m);

	/** Used by this class internally during std::sort and 005 generation
	 */
	static bool PrefixComparison(prefixtype one, prefixtype two);

	/** This returns the PREFIX=(ohv)@%+ section of the 005 numeric.
	 */
	std::string BuildPrefixes();

	/** This returns the privilages of a user upon a channel, in the format of a mode change.
	 * For example, if a user has privilages +avh, this will return the string "avh nick nick nick".
	 * This is used by the core when cycling a user to refresh their hostname. You may use it for
	 * similar purposes.
	 * @param user The username to look up
	 * @param channel The channel name to look up the privilages of the user for
	 * @param nick_suffix true (the default) if you want nicknames in the mode string, for easy
	 * use with the mode stacker, false if you just want the "avh" part of "avh nick nick nick".
	 * @return The mode string.
	 */
	std::string ModeString(User* user, Channel* channel, bool nick_suffix = true);
};

#endif
