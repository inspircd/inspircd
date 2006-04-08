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

#ifndef __CMD_CONNECT_H__
#define __CMD_CONNECT_H__

#include "users.h"
#include "channels.h"
#include "ctables.h"

class cmd_connect : public command_t
{
 public:
        cmd_connect () : command_t("CONNECT",'o',1) { }
        void Handle(char **parameters, int pcnt, userrec *user);
};

#endif
