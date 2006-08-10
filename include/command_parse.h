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

class InspIRCd;

class CommandParser : public classbase
{
 private:
	InspIRCd* ServerInstance;

	int ProcessParameters(char **command_p,char *parameters);
	void ProcessCommand(userrec *user, std::string &cmd);
	void SetupCommandTable();
 public:
	command_table cmdlist;

	CommandParser(InspIRCd* Instance);
	bool CallHandler(const std::string &commandname,const char** parameters, int pcnt, userrec *user);
	bool IsValidCommand(const std::string &commandname, int pcnt, userrec * user);
	int LoopCall(userrec* user, command_t* CommandObj, const char** parameters, int pcnt, unsigned int splithere, unsigned int extra);
	int LoopCall(userrec* user, command_t* CommandObj, const char** parameters, int pcnt, unsigned int splithere);
	void ProcessBuffer(std::string &buffer,userrec *user);
	bool RemoveCommands(const char* source);
	bool CreateCommand(command_t *f);
};

#endif
