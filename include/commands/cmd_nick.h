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

#ifndef __CMD_NICK_H__
#define __CMD_NICK_H__

// include the common header files

#include "users.h"
#include "channels.h"

/** Handle /NICK
 */
class cmd_nick : public command_t
{
 public:
        cmd_nick (InspIRCd* Instance) : command_t(Instance,"NICK",0,1,true) { syntax = "<newnick>"; }
        CmdResult Handle(const char** parameters, int pcnt, userrec *user);
};

#endif
