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

class cmd_mode : public command_t
{
 public:
        cmd_mode (InspIRCd* Instance) : command_t(Instance,"MODE",0,1) { syntax = "<target> <modes> {<mode-parameters>}"; }
        CmdResult Handle(const char** parameters, int pcnt, userrec *user);
};

#endif
