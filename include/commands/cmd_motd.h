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

#ifndef __CMD_MOTD_H__
#define __CMD_MOTD_H__

// include the common header files

#include <string>
#include <vector>
#include "inspircd.h"
#include "users.h"
#include "channels.h"

class cmd_motd : public command_t
{
 public:
        cmd_motd (InspIRCd* Instance) : command_t(Instance,"MOTD",0,0) { syntax = "[<servername>]"; }
        void Handle(const char** parameters, int pcnt, userrec *user);
};

#endif
