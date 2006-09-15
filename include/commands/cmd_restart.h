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

#ifndef __CMD_RESTART_H__
#define __CMD_RESTART_H__

// include the common header files

#include <string>
#include <deque>
#include <vector>
#include "users.h"
#include "channels.h"

/** Handle /RESTART
 */
class cmd_restart : public command_t
{
 public:
        cmd_restart (InspIRCd* Instance) : command_t(Instance,"RESTART",'o',1) { syntax = "<password>"; }
        CmdResult Handle(const char** parameters, int pcnt, userrec *user);
};

#endif
