/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2005 ChatSpike-Dev.
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
 
class CommandParser
{
 public:
	void CallHandler(std::string &commandname,char **parameters, int pcnt, userrec *user);
	bool IsValidCommand(std::string &commandname, int pcnt, userrec * user);
	int LoopCall(handlerfunc fn, char **parameters, int pcnt, userrec *u, int start, int end, int joins);
	void ProcessBuffer(const char* cmdbuf,userrec *user);
	bool RemoveCommands(const char* source);
	void CommandParser::ProcessCommand(userrec *user, char* cmd);
};

#endif
