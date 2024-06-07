/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019, 2021, 2023 Sadie Powell <sadie@witchery.services>
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

namespace Names
{
	class EventListener;
}

class Names::EventListener
	: public Events::ModuleEventListener
{
public:
	EventListener(Module* mod, unsigned int eventprio = DefaultPriority)
		: ModuleEventListener(mod, "event/names", eventprio)
	{
	}

	/* Called for every item in a NAMES list.
	 * @param issuer The user who initiated the NAMES request.
	 * @param memb The channel membership of the user who is being considered for inclusion.
	 * @param prefixes The prefix character(s) to show in front of the user's nickname.
	 * @param nick The nickname of the user to show.
	 * @return Return MOD_RES_PASSTHRU to allow the member to be displayed, MOD_RES_DENY to cause them to be
	 * excluded from this NAMES list
	 */
	virtual ModResult OnNamesListItem(LocalUser* issuer, Membership* memb, std::string& prefixes, std::string& nick) = 0;
};
