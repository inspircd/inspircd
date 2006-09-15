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

#ifndef __CMD_ISON_H__
#define __CMD_ISON_H__

// include the common header files

#include "users.h"
#include "channels.h"

/** Handle /ISON
 */
class cmd_ison : public command_t
{
 public:
        cmd_ison (InspIRCd* Instance) : command_t(Instance,"ISON",0,0) { syntax = "<nick> {nick}"; }
        CmdResult Handle(const char** parameters, int pcnt, userrec *user);
};

#endif
