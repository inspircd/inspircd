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

#include "users.h"
#include "commands.h"
#include "helperfuncs.h"
#include "inspircd.h"
#include "commands/cmd_lusers.h"

extern InspIRCd* ServerInstance;

void cmd_lusers::Handle (const char** parameters, int pcnt, userrec *user)
{
	// this lusers command shows one server at all times because
	// a protocol module must override it to show those stats.
	user->WriteServ("251 %s :There are %d users and %d invisible on 1 server",user->nick,ServerInstance->usercnt()-ServerInstance->usercount_invisible(),ServerInstance->usercount_invisible());
	if (ServerInstance->usercount_opers())
		user->WriteServ("252 %s %d :operator(s) online",user->nick,ServerInstance->usercount_opers());
	if (ServerInstance->usercount_unknown())
		user->WriteServ("253 %s %d :unknown connections",user->nick,ServerInstance->usercount_unknown());
	if (ServerInstance->chancount())
		user->WriteServ("254 %s %d :channels formed",user->nick,ServerInstance->chancount());
	if (ServerInstance->local_count())
		user->WriteServ("254 %s :I have %d clients and 0 servers",user->nick,ServerInstance->local_count());
}

