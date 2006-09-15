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

#ifndef __CMD_PING_H__
#define __CMD_PING_H__

// include the common header files

#include "users.h"
#include "channels.h"

/** Handle /PING
 */
class cmd_ping : public command_t
{
 public:
        cmd_ping (InspIRCd* Instance) : command_t(Instance,"PING",0,1) { syntax = "<servername> [:<servername>]"; }
        CmdResult Handle(const char** parameters, int pcnt, userrec *user);
};

#endif
