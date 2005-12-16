/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
 *                       E-mail:
 *                <brain.net>
 *                <Craig.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __CMD_RESTART_H__
#define __CMD_RESTART_H__

// include the common header files

#include <typeinfo>
#include <iostream>
#include <string>
#include <deque>
#include <sstream>
#include <vector>
#include "users.h"
#include "channels.h"

class cmd_restart : public command_t
{
 public:
        cmd_restart () : command_t("RESTART",'o',1) { }
        void Handle(char **parameters, int pcnt, userrec *user);
};

#endif
