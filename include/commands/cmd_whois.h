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

#include "users.h"
#include "channels.h"

const char* Spacify(char* n);
void do_whois(InspIRCd* Instance, userrec* user, userrec* dest,unsigned long signon, unsigned long idle, const char* nick);

class cmd_whois : public command_t
{
 public:
        cmd_whois (InspIRCd* Instance) : command_t(Instance,"WHOIS",0,1) { syntax = "<nick>{,<nick>}"; }
        CmdResult Handle(const char** parameters, int pcnt, userrec *user);
};

#endif
