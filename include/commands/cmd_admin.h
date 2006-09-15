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

#ifndef __CMD_ADMIN_H__
#define __CMD_ADMIN_H__

#include "users.h"
#include "channels.h"
#include "ctables.h"

/** Handle /ADMIN
 */
class cmd_admin : public command_t
{
 public:
        cmd_admin (InspIRCd* Instance) : command_t(Instance,"ADMIN",0,0) { syntax = "[<servername>]"; }
        CmdResult Handle(const char** parameters, int pcnt, userrec *user);
};

#endif
