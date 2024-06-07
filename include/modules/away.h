/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018, 2021, 2023 Sadie Powell <sadie@witchery.services>
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

namespace Away
{
	class EventListener;
	class EventProvider;
}

class Away::EventListener
	: public Events::ModuleEventListener
{
protected:
	EventListener(Module* mod, unsigned int eventprio = DefaultPriority)
		: ModuleEventListener(mod, "event/away", eventprio)
	{
	}

public:
	/** Called when a user wishes to mark themselves as away.
	 * @param user The user who is going away.
	 * @param message The away message that the user set.
	 * @return Either MOD_RES_ALLOW to allow the user to mark themself as away, MOD_RES_DENY to
	 *         disallow the user to mark themself as away, or MOD_RES_PASSTHRU to let another module
	 *         handle the event.
	 */
	virtual ModResult OnUserPreAway(LocalUser* user, std::string& message)
	{
		return MOD_RES_PASSTHRU;
	}

	/** Called when a user wishes to mark themselves as back.
	 * @param user The user who is going away.
	 * @return Either MOD_RES_ALLOW to allow the user to mark themself as back, MOD_RES_DENY to
	 *         disallow the user to mark themself as back, or MOD_RES_PASSTHRU to let another module
	 *         handle the event.
	 */
	virtual ModResult OnUserPreBack(LocalUser* user)
	{
		return MOD_RES_PASSTHRU;
	}

	/** Called when a user has marked themself as away.
	 * @param user The user who has gone away.
	 * @param prevstate The previous away state of the user.
	 */
	virtual void OnUserAway(User* user, const std::optional<AwayState>& prevstate) = 0;

	/** Called when a user has returned from being away.
	 * @param user The user who has returned from being away.
	 * @param prevstate The previous away state of the user.
	 */
	virtual void OnUserBack(User* user, const std::optional<AwayState>& prevstate) = 0;
};

class Away::EventProvider final
	: public Events::ModuleEventProvider
{
public:
	EventProvider(Module* mod)
		: ModuleEventProvider(mod, "event/away")
	{
	}
};
