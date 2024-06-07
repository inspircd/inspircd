/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017, 2019, 2021, 2023 Sadie Powell <sadie@witchery.services>
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

namespace CheckExemption
{
	class EventListener;
	class EventProvider;
}

class CheckExemption::EventListener
	: public Events::ModuleEventListener
{
protected:
	EventListener(Module* mod, unsigned int eventprio = DefaultPriority)
		: ModuleEventListener(mod, "event/exemption", eventprio)
	{
	}

public:
	/** Called when checking if a user is exempt from something.
	 * @param user The user to check exemption for.
	 * @param chan The channel to check exemption on.
	 * @param restriction The restriction to check for.
	 * @return Either MOD_RES_ALLOW to confirm an exemption, MOD_RES_DENY to deny an exemption,
	 *         or MOD_RES_PASSTHRU to let another module handle the event.
	 */
	virtual ModResult OnCheckExemption(User* user, Channel* chan, const std::string& restriction) = 0;
};

class CheckExemption::EventProvider final
	: public Events::ModuleEventProvider
{
public:
	EventProvider(Module* mod)
		: ModuleEventProvider(mod, "event/exemption")
	{
	}

	/** Helper function for calling the CheckExemption::EventListener::OnCheckExemption event.
	 * @param user The user to check exemption for.
	 * @param chan The channel to check exemption on.
	 * @param restriction The restriction to check for.
	 * @return Either MOD_RES_ALLOW if the exemption was confirmed, MOD_RES_DENY if the exemption was
	 *         denied or MOD_RES_PASSTHRU if no module handled the event.
	 */
	inline ModResult Check(User* user, Channel* chan, const std::string& restriction)
	{
		return FirstResult(&CheckExemption::EventListener::OnCheckExemption, user, chan, restriction);
	}
};
