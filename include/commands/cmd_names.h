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

#ifndef __CMD_NAMES_H__
#define __CMD_NAMES_H__

// include the common header files

#include "users.h"
#include "channels.h"

/** Handle /NAMES
 */
class cmd_names : public command_t
{
 public:
        cmd_names (InspIRCd* Instance) : command_t(Instance,"NAMES",0,0) { syntax = "{<channel>{,<channel>}}"; }
        CmdResult Handle(const char** parameters, int pcnt, userrec *user);
};

#endif
