/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
	bool opt_time;

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
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
	bool whomatch(User* cuser, User* user, const char* matchtext);
};

#endif
