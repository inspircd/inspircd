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
	UM_WALLOPS = 3
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
