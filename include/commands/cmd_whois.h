/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *      the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __CMD_WHOIS_H__
#define __CMD_WHOIS_H__

// include the common header files

#include "users.h"
#include "channels.h"

const char* Spacify(char* n);
DllExport void do_whois(InspIRCd* Instance, userrec* user, userrec* dest,unsigned long signon, unsigned long idle, const char* nick);

/** Handle /WHOIS. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class cmd_whois : public command_t
{
 public:
	/** Constructor for whois.
	 */
	cmd_whois (InspIRCd* Instance) : command_t(Instance,"WHOIS",0,1) { syntax = "<nick>{,<nick>}"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const char** parameters, int pcnt, userrec *user);
};

#endif
