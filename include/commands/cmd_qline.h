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

#ifndef __CMD_QLINE_H__
#define __CMD_QLINE_H__

// include the common header files

#include "users.h"
#include "channels.h"

class cmd_qline : public command_t
{
 public:
        cmd_qline (InspIRCd* Instance) : command_t(Instance,"QLINE",'o',1) { syntax = "<nick> [<duration> :<reason>]"; }
        CmdResult Handle(const char** parameters, int pcnt, userrec *user);
};

#endif
