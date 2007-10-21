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

#ifndef __CMD_WHO_H__
#define __CMD_WHO_H__

// include the common header files

#include "users.h"
#include "channels.h"

/** Handle /WHO. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandWho : public Command
{
	bool CanView(Channel* chan, User* user);
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
	/** Constructor for who.
	 */
	CommandWho (InspIRCd* Instance) : Command(Instance,"WHO", 0, 1, false, 2) { syntax = "<server>|<nickname>|<channel>|<realname>|<host>|0 [ohurmMiaplf]"; }
	void SendWhoLine(User* user, const std::string &initial, Channel* ch, User* u, std::vector<std::string> &whoresults);
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const char** parameters, int pcnt, User *user);
	bool whomatch(User* user, const char* matchtext);
};

#endif
