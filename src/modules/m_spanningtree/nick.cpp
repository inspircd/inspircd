/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2007-2008, 2012 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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


#include "inspircd.h"

#include "main.h"
#include "utils.h"
#include "commands.h"

CmdResult CommandNick::Handle(User* user, std::vector<std::string>& params)
{
	if (IS_SERVER(user))
		return CMD_INVALID;

	if ((isdigit(params[0][0])) && (params[0] != user->uuid))
		return CMD_INVALID;

	/* Update timestamp on user when they change nicks */
	user->age = ConvToInt(params[1]);

	/*
	 * On nick messages, check that the nick doesn't already exist here.
	 * If it does, perform collision logic.
	 */
	User* x = ServerInstance->FindNickOnly(params[0]);
	if ((x) && (x != user))
	{
		/* x is local, who is remote */
		int collideret = Utils->DoCollision(x, Utils->FindServer(user->server), user->age, user->ident, user->GetIPString(), user->uuid);
		if (collideret != 1)
		{
			/*
			 * Remote client lost, or both lost, parsing or passing on this
			 * nickchange would be pointless, as the incoming client's server will
			 * soon receive SAVE to change its nick to its UID. :) -- w00t
			 */
			return CMD_FAILURE;
		}
	}
	user->ForceNickChange(params[0]);
	return CMD_SUCCESS;
}
