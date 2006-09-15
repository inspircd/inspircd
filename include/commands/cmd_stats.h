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

#ifndef __CMD_STATS_H__
#define __CMD_STATS_H__

// include the common header files

#include "inspircd.h"
#include "users.h"
#include "channels.h"

void DoStats(InspIRCd* Instance, char statschar, userrec* user, string_list &results);

/** Handle /STATS
 */
class cmd_stats : public command_t
{
 public:
        cmd_stats (InspIRCd* Instance) : command_t(Instance,"STATS",0,1) { syntax = "[<servername>] <stats-symbol>"; }
        CmdResult Handle(const char** parameters, int pcnt, userrec *user);
};

#endif
