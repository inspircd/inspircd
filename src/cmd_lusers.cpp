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
#include "commands/cmd_lusers.h"

void cmd_lusers::Handle (const char** parameters, int pcnt, userrec *user)
{
	// this lusers command shows one server at all times because
	// a protocol module must override it to show those stats.
	WriteServ(user->fd,"251 %s :There are %d users and %d invisible on 1 server",user->nick,usercnt()-usercount_invisible(),usercount_invisible());
	WriteServ(user->fd,"252 %s %d :operator(s) online",user->nick,usercount_opers());
	WriteServ(user->fd,"253 %s %d :unknown connections",user->nick,usercount_unknown());
	WriteServ(user->fd,"254 %s %d :channels formed",user->nick,chancount());
	WriteServ(user->fd,"254 %s :I have %d clients and 0 servers",user->nick,local_count());
}
