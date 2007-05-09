/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *		       E-mail:
 *		<brain@chatspike.net>
 *		<Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __CMD_WHO_H__
#define __CMD_WHO_H__

// include the common header files

#include "users.h"
#include "channels.h"

/** Handle /WHO
 */
class cmd_who : public command_t
{
	bool CanView(chanrec* chan, userrec* user);
	bool opt_viewopersonly;
	bool opt_showrealhost;
	bool opt_unlimit;
	bool opt_realname;
	bool opt_mode;
	bool opt_ident;
	bool opt_metadata;
	bool opt_port;
	bool opt_away;
	bool opt_local;
	bool opt_far;
 public:
	cmd_who (InspIRCd* Instance) : command_t(Instance,"WHO",0,1) { syntax = "<server>|<nickname>|<channel>|<realname>|<host>|0 [ohurmMiaplf]"; }
	void SendWhoLine(userrec* user, const std::string &initial, chanrec* ch, userrec* u, std::vector<std::string> &whoresults);
	CmdResult Handle(const char** parameters, int pcnt, userrec *user);
	bool whomatch(userrec* user, const char* matchtext);
};

#endif
