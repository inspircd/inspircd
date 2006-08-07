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

#ifndef __CMD_OPER_H__
#define __CMD_OPER_H__

// include the common header files

#include <typeinfo>
#include <iostream>
#include <string>
#include <deque>
#include <sstream>
#include <vector>
#include "users.h"
#include "channels.h"

bool OneOfMatches(const char* host, const char* ip, const char* hostlist);

class cmd_oper : public command_t
{
 public:
        cmd_oper () : command_t("OPER",0,2) { syntax = "<username> <password>"; }
        void Handle(const char** parameters, int pcnt, userrec *user);
};

#endif
