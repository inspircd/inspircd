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

enum UserModeBits {
	UM_INVISIBLE = 1,
	UM_SERVERNOTICE = 2,
	UM_WALLOPS = 4
};

enum ModeType {
	MODETYPE_USER = 0,
	MODETYPE_CHANNEL = 1
};

enum ModeAction {
	MODEACTION_DENY = 0, /* Drop the mode change, AND a parameter if its a parameterized mode */
	MODEACTION_ALLOW = 1 /* Allow the mode */
};

enum ModeMasks {
	MASK_USER = 128,	/* A user mode */
	MASK_CHANNEL = 0	/* A channel mode */
};

class ModeHandler
{
	char mode;
	int n_params_on;
	int n_params_off;
	bool list;
	ModeType m_type;
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
	 * or a channel mode, so we have 255 of them not 64.
	 */
	ModeHandler* modehandlers[256];
	/**
	 * Mode watcher classes
	 */
	std::vector<ModeWatcher*> modewatchers[256];
	
	char* GiveOps(userrec *user,char *dest,chanrec *chan,int status);
	char* GiveHops(userrec *user,char *dest,chanrec *chan,int status);
	char* GiveVoice(userrec *user,char *dest,chanrec *chan,int status);
	char* TakeOps(userrec *user,char *dest,chanrec *chan,int status);
	char* TakeHops(userrec *user,char *dest,chanrec *chan,int status);
	char* TakeVoice(userrec *user,char *dest,chanrec *chan,int status);
	char* AddBan(userrec *user,char *dest,chanrec *chan,int status);
	char* TakeBan(userrec *user,char *dest,chanrec *chan,int status);
	userrec* SanityChecks(userrec *user,char *dest,chanrec *chan,int status);
	char* Grant(userrec *d,chanrec *chan,int MASK);
	char* Revoke(userrec *d,chanrec *chan,int MASK);
 public:
	ModeParser();

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
