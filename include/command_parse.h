/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __COMMAND_PARSE_H
#define __COMMAND_PARSE_H

#include <typeinfo>
#include <iostream>
#include <string>
#include "users.h"
#include "ctables.h"
#include "typedefs.h"
 
class CommandParser
{
 private:
	int ProcessParameters(char **command_p,char *parameters);
	void ProcessCommand(userrec *user, char* cmd);
	void SetupCommandTable();
 public:
	command_table cmdlist;

	CommandParser();
	bool CallHandler(std::string &commandname,char **parameters, int pcnt, userrec *user);
	bool IsValidCommand(std::string &commandname, int pcnt, userrec * user);
	int LoopCall(command_t *fn, char **parameters, int pcnt, userrec *u, int start, int end, int joins);
	void ProcessBuffer(const char* cmdbuf,userrec *user);
	bool RemoveCommands(const char* source);
	bool CreateCommand(command_t *f);
};

#endif
