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

#ifndef __CMD_KLINE_H__
#define __CMD_KLINE_H__

// include the common header files

#include "users.h"
#include "channels.h"

/** Handle /KLINE. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class cmd_kline : public command_t
{
 public:
	/** Constructor for kline.
	 */
	cmd_kline (InspIRCd* Instance) : command_t(Instance,"KLINE",'o',1) { syntax = "<ident@host> [<duration> :<reason>]"; }
	CmdResult Handle(const char** parameters, int pcnt, userrec *user);
};

#endif
