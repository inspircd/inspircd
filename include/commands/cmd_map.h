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

#ifndef __CMD_MAP_H__
#define __CMD_MAP_H__

// include the common header files

#include "users.h"
#include "channels.h"

/** Handle /MAP
 */
class cmd_map : public command_t
{
 public:
        cmd_map (InspIRCd* Instance) : command_t(Instance,"MAP",0,0) { }
        CmdResult Handle(const char** parameters, int pcnt, userrec *user);
};

#endif
