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

#ifndef __CMD_WHOIS_H__
#define __CMD_WHOIS_H__

// include the common header files

#include <typeinfo>
#include <iostream>
#include <string>
#include <deque>
#include <sstream>
#include <vector>
#include "users.h"
#include "channels.h"

void do_whois(userrec* user, userrec* dest,unsigned long signon, unsigned long idle, const char* nick);

class cmd_whois : public command_t
{
 public:
        cmd_whois () : command_t("WHOIS",0,1) { }
        void Handle(char **parameters, int pcnt, userrec *user);
};

#endif
