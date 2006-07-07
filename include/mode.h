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
 * This enum contains a set of bitmasks which
 * are used to compress the 'standard' usermodes
 * +isw into a bitfield for fast checking.
 */
enum UserModeBits {
	UM_INVISIBLE = 1,
	UM_SERVERNOTICE = 2,
	UM_WALLOPS = 4
};

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
	ModeHandler(char modeletter, int parameters_on, int parameters_off, bool listmode, ModeType type, bool operonly);
	virtual ~ModeHandler();

	bool IsListMode();
	ModeType GetModeType();
	bool NeedsOper();
	int GetNumParams(bool adding);
	char GetModeChar();

	virtual ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding); /* Can change the mode parameter as its a ref */
	virtual void DisplayList(userrec* user, chanrec* channel);
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
	
	char* GiveOps(userrec *user,char *dest,chanrec *chan,int status);
	char* GiveHops(userrec *user,char *dest,chanrec *chan,int status);
	char* GiveVoice(userrec *user,char *dest,chanrec *chan,int status);
	char* TakeOps(userrec *user,char *dest,chanrec *chan,int status);
	char* TakeHops(userrec *user,char *dest,chanrec *chan,int status);
	char* TakeVoice(userrec *user,char *dest,chanrec *chan,int status);
	userrec* SanityChecks(userrec *user,char *dest,chanrec *chan,int status);
	char* Grant(userrec *d,chanrec *chan,int MASK);
	char* Revoke(userrec *d,chanrec *chan,int MASK);
 public:
	ModeParser();
	bool AddMode(ModeHandler* mh, unsigned const char modeletter);
	void Process(char **parameters, int pcnt, userrec *user, bool servermode);

	//void ServerMode(char **parameters, int pcnt, userrec *user);
};

class cmd_mode : public command_t
{
 public:
	cmd_mode () : command_t("MODE",0,1) { }
	void Handle(char **parameters, int pcnt, userrec *user);
};

#endif
