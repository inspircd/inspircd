/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2007 ChatSpike-Dev.
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

#ifndef __CMD_NOTICE_H__
#define __CMD_NOTICE_H__

// include the common header files

#include "users.h"
#include "channels.h"

/** Handle /NOTICE. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class cmd_notice : public command_t
{
 public:
	/** Constructor for notice.
	 */
	cmd_notice (InspIRCd* Instance) : command_t(Instance,"NOTICE",0,2) { syntax = "<target>{,<target>} <message>"; }
	CmdResult Handle(const char** parameters, int pcnt, userrec *user);
};

#endif
