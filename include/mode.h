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

#ifndef __MODE_H
#define __MODE_H

/* include the common header files */
#include <typeinfo>
#include <iostream>
#include <string>
#include <deque>
#include <sstream>
#include <vector>
#include "users.h"
#include "channels.h"
#include "ctables.h"

class InspIRCd;

/**
 * Holds the values for different type of modes
 * that can exist, USER or CHANNEL type.
 */
enum ModeType {
	MODETYPE_USER = 0,
	MODETYPE_CHANNEL = 1
};

/**
 * Holds mode actions - modes can be allowed or denied.
 */
enum ModeAction {
	MODEACTION_DENY = 0, /* Drop the mode change, AND a parameter if its a parameterized mode */
	MODEACTION_ALLOW = 1 /* Allow the mode */
};

/**
 * Used to mask off the mode types in the mode handler
 * array. Used in a simple two instruction hashing function
 * "(modeletter - 65) OR mask"
 */
enum ModeMasks {
	MASK_USER = 128,	/* A user mode */
	MASK_CHANNEL = 0	/* A channel mode */
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
class ModeHandler : public Extensible
{
 protected:
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
	 * True if the mode requires oper status
	 * to set.
	 */
	bool oper;

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
	 */
	ModeHandler(InspIRCd* Instance, char modeletter, int parameters_on, int parameters_off, bool listmode, ModeType type, bool operonly);
	/**
	 * The default destructor does nothing
	 */
	virtual ~ModeHandler();

	/**
	 * Returns true if the mode is a list mode
	 */
	bool IsListMode();
	/**
	 * Returns the modes type
	 */
	ModeType GetModeType();
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
	virtual ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding); /* Can change the mode parameter as its a ref */
	/**
	 * If your mode is a listmode, then this method will be called for displaying an item list, e.g. on MODE #channel +modechar
	 * without any parameter or other modes in the command.
	 * @param user The user issuing the command
	 * @parameter channel The channel they're requesting an item list of (e.g. a banlist, or an exception list etc)
	 */
	virtual void DisplayList(userrec* user, chanrec* channel);
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
	virtual bool CheckTimeStamp(time_t theirs, time_t ours, const std::string &their_param, const std::string &our_param, chanrec* channel);

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
	virtual ModePair ModeSet(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter);
};

/**
 * The ModeWatcher class can be used to alter the behaviour of a mode implemented
 * by the core or by another module. To use ModeWatcher, derive a class from it,
 * and attach it to the mode using Server::AddModeWatcher and Server::DelModeWatcher.
 * A ModeWatcher will be called both before and after the mode change.
 */
class ModeWatcher : public Extensible
{
 protected:
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
	virtual bool BeforeMode(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding, ModeType type);
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
	virtual void AfterMode(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter, bool adding, ModeType type);
};

typedef std::vector<ModeWatcher*>::iterator ModeWatchIter;

/** The mode parser handles routing of modes and handling of mode strings.
 * It marshalls, controls and maintains both ModeWatcher and ModeHandler classes,
 * parses client to server MODE strings for user and channel modes, and performs
 * processing for the 004 mode list numeric, amongst other things.
 */
class ModeParser : public classbase
{
 private:
	InspIRCd* ServerInstance;
	/**
	 * Mode handlers for each mode, to access a handler subtract
	 * 65 from the ascii value of the mode letter.
	 * The upper bit of the value indicates if its a usermode
	 * or a channel mode, so we have 256 of them not 64.
	 */
	ModeHandler* modehandlers[256];
	/**
	 * Mode watcher classes arranged in the same way as the
	 * mode handlers, except for instead of having 256 of them
	 * we have 256 lists of them.
	 */
	std::vector<ModeWatcher*> modewatchers[256];
	/**
	 * Displays the current modes of a channel or user.
	 * Used by ModeParser::Process.
	 */
	void DisplayCurrentModes(userrec *user, userrec* targetuser, chanrec* targetchannel, const char* text);

 public:

	/**
	 * The constructor initializes all the RFC basic modes by using ModeParserAddMode().
	 */
	ModeParser(InspIRCd* Instance);

	/**
	 * Used to check if user 'd' should be allowed to do operation 'MASK' on channel 'chan'.
	 * for example, should 'user A' be able to 'op' on 'channel B'.
	 */
	userrec* SanityChecks(userrec *user,const char *dest,chanrec *chan,int status);
	/**
	 * Grant a built in privilage (e.g. ops, halfops, voice) to a user on a channel
	 */
	const char* Grant(userrec *d,chanrec *chan,int MASK);
	/**
	 * Revoke a built in privilage (e.g. ops, halfops, voice) to a user on a channel
	 */
	const char* Revoke(userrec *d,chanrec *chan,int MASK);
	/**
	 * Tidy a banmask. This makes a banmask 'acceptable' if fields are left out.
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
	/**
	 * Add a mode to the mode parser. The modeletter parameter
	 * is purely to save on doing a lookup in the function, as
	 * strictly it could be obtained via ModeHandler::GetModeChar().
	 * @return True if the mode was successfully added.
	 */
	bool AddMode(ModeHandler* mh, unsigned const char modeletter);
	/**
	 * Add a mode watcher.
	 * A mode watcher is triggered before and after a mode handler is
	 * triggered. See the documentation of class ModeWatcher for more
	 * information.
	 * @param mw The ModeWatcher you want to add
	 * @return True if the ModeWatcher was added correctly
	 */
	bool AddModeWatcher(ModeWatcher* mw);
	/**
	 * Delete a mode watcher.
	 * A mode watcher is triggered before and after a mode handler is
	 * triggered. See the documentation of class ModeWatcher for more
	 * information.
	 * @param mw The ModeWatcher you want to delete
	 * @return True if the ModeWatcher was deleted correctly
	 */
	bool DelModeWatcher(ModeWatcher* mw);
	/**
	 * Process a set of mode changes from a server or user.
	 * @param parameters The parameters of the mode change, in the format
	 * they would be from a MODE command.
	 * @param pcnt The number of items in the parameters array
	 * @param user The user setting or removing the modes. When the modes are set
	 * by a server, an 'uninitialized' userrec is used, where *user::nick == NULL
	 * and *user->server == NULL.
	 * @param servermode True if a server is setting the mode.
	 */
	void Process(const char** parameters, int pcnt, userrec *user, bool servermode);

	/**
	 * Find the mode handler for a given mode and type
	 * @param modeletter mode letter to search for
	 * @param type of mode to search for, user or channel
	 * @returns a pointer to a ModeHandler class, or NULL of there isnt a handler for the given mode
	 */
	ModeHandler* FindMode(unsigned const char modeletter, ModeType mt);

	std::string UserModeList();

	std::string ChannelModeList();

	std::string ParaModeList();

	bool InsertMode(std::string &output, const char* mode, unsigned short section);
};

/**
 * Command handler class for the MODE command.
 * put here for completeness.
 */
class cmd_mode : public command_t
{
 public:
	/**
	 * Standard constructor
	 */
	cmd_mode (InspIRCd* Instance) : command_t(Instance,"MODE",0,1) { syntax = "<target> <modes> {<mode-parameters>}"; }
	/**
	 * Handle MODE
	 */
	void Handle(const char** parameters, int pcnt, userrec *user);
};

#endif
