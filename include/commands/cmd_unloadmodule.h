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

#ifndef __CMD_UNLOADMODULE_H__
#define __CMD_UNLOADMODULE_H__

// include the common header files

#include "users.h"
#include "channels.h"

class cmd_unloadmodule : public command_t
{
 public:
        cmd_unloadmodule (InspIRCd* Instance) : command_t(Instance,"UNLOADMODULE",'o',1) { syntax = "<modulename>"; }
        CmdResult Handle(const char** parameters, int pcnt, userrec *user);
};

#endif
