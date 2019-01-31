/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Dylan Frank <b00mx0r@aureus.pw>
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


#pragma once

#include "event.h"

class HideListEventListener : public Events::ModuleEventListener
{
 public:
	HideListEventListener(Module* mod)
		: ModuleEventListener(mod, "event/hidelist")
	{
	}

	/** Called when an attempt to display a listmode will be denied
	 * @param user The user executing the list
	 * @param chan The channel the list is being requested on
	 * @param modename The name of the mode being listed
	 */
	virtual ModResult OnListDeny(User* user, Channel* chan, const std::string& modename) = 0;
};
