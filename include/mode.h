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

// include the common header files

#include <typeinfo>
#include <iostream>
#include <string>
#include <deque>
#include <sstream>
#include <vector>
#include "users.h"
#include "channels.h"
#include "ctables.h"

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
class ModeHandler
{
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
	ModeHandler(char modeletter, int parameters_on, int parameters_off, bool listmode, ModeType type, bool operonly);
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
	 * theirs < ours.
	 * @param theirs The timestamp of the remote side
	 * @param ours The timestamp of the local side
	 * @param their_param Their parameter if the mode has a parameter
	 * @param our_param Our parameter if the mode has a parameter
	 * @param channel The channel we are checking against
	 * @return True if the other side wins the merge, false if we win the merge for this mode.
	 */
	virtual bool CheckTimeStamp(time_t theirs, time_t ours, const std::string &their_param, const std::string &our_param, chanrec* channel);
};

class ModeWatcher
{
	char mode;
	ModeType m_type;

 public:
	ModeWatcher(char modeletter, ModeType type);
	virtual ~ModeWatcher();

	char GetModeChar();
	ModeType GetModeType();

	virtual bool BeforeMode(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding, ModeType type); /* Can change the mode parameter */
	virtual void AfterMode(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter, bool adding, ModeType type);
};

typedef std::vector<ModeWatcher*>::iterator ModeWatchIter;

class ModeParser
{
 private:
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

	/*char* GiveHops(userrec *user,char *dest,chanrec *chan,int status);
	char* GiveVoice(userrec *user,char *dest,chanrec *chan,int status);
	char* TakeHops(userrec *user,char *dest,chanrec *chan,int status);
	char* TakeVoice(userrec *user,char *dest,chanrec *chan,int status);*/

 public:

	ModeParser();

	static userrec* SanityChecks(userrec *user,const char *dest,chanrec *chan,int status);
	static const char* Grant(userrec *d,chanrec *chan,int MASK);
	static const char* Revoke(userrec *d,chanrec *chan,int MASK);
	static void CleanMask(std::string &mask);
	static void BuildModeString(userrec* user);

	bool AddMode(ModeHandler* mh, unsigned const char modeletter);
	void Process(char **parameters, int pcnt, userrec *user, bool servermode);
};

class cmd_mode : public command_t
{
 public:
	cmd_mode () : command_t("MODE",0,1) { }
	void Handle(char **parameters, int pcnt, userrec *user);
};

#endif
