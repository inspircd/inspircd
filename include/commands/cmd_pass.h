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

#ifndef __CMD_PASS_H__
#define __CMD_PASS_H__

// include the common header files

#include <string>
#include <vector>
#include "inspircd.h"
#include "users.h"
#include "channels.h"

class cmd_pass : public command_t
{
 public:
        cmd_pass (InspIRCd* Instance) : command_t(Instance,"PASS",0,1,true) { syntax = "<password>"; }
        CmdResult Handle(const char** parameters, int pcnt, userrec *user);
};

#endif
