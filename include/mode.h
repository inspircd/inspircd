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

class ModeOutput
{
 private:
	std::string par;
	ModeAction act;
 public:
	ModeOutput(std::string parameter, ModeAction action);
	ModeAction GetAction();
	std::string& GetParameter();
};

class ModeHandler
{
	char mode;
	int n_params;
	bool list;
	ModeType m_type;
	bool oper;

 public:
	ModeHandler(char modeletter, int parameters, bool listmode, ModeType type, bool operonly);
	virtual ~ModeHandler();

	bool IsListMode();
	ModeType GetModeType();
	bool NeedsOper();
	int GetNumParams();
	char GetModeChar();

	virtual ModeOutput OnModeChange(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter, bool adding);
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

	virtual bool BeforeMode(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter, bool adding);
	virtual void AfterMode(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter, bool adding);
};

class ModeParser
{
 private:
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
	std::string CompressModes(std::string modes,bool channelmodes);
	void ProcessModes(char **parameters,userrec* user,chanrec *chan,int status, int pcnt, bool servermode, bool silent, bool local);
	bool AllowedUmode(char umode, char* sourcemodes,bool adding,bool serveroverride);
	bool ProcessModuleUmode(char umode, userrec* source, void* dest, bool adding);
	void ServerMode(char **parameters, int pcnt, userrec *user);
};

class cmd_mode : public command_t
{
 public:
	cmd_mode () : command_t("MODE",0,1) { }
	void Handle(char **parameters, int pcnt, userrec *user);
};

#endif
