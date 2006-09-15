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

#ifndef __CMD_ELINE_H__
#define __CMD_ELINE_H__

// include the common header files

#include "users.h"
#include "channels.h"

/** Handle /ELINE
 */
class cmd_eline : public command_t
{
 public:
        cmd_eline (InspIRCd* Instance) : command_t(Instance,"ELINE",'o',1) { syntax = "<ident@host> [<duration> :<reason>]"; }
        CmdResult Handle(const char** parameters, int pcnt, userrec *user);
};

#endif
