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
 
void call_handler(std::string &commandname,char **parameters, int pcnt, userrec *user);
bool is_valid_cmd(std::string &commandname, int pcnt, userrec * user);
int loop_call(handlerfunc fn, char **parameters, int pcnt, userrec *u, int start, int end, int joins);
void process_buffer(const char* cmdbuf,userrec *user);

#endif
