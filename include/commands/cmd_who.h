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

#ifndef __CMD_WHO_H__
#define __CMD_WHO_H__

// include the common header files

#include "users.h"
#include "channels.h"

class cmd_who : public command_t
{
 public:
        cmd_who (InspIRCd* Instance) : command_t(Instance,"WHO",0,1) { syntax = "<server>|<nickname>|<channel>|<realname>|<host>|0 [ohur]"; }
        void Handle(const char** parameters, int pcnt, userrec *user);
};

#endif
