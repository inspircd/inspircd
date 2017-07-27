/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2016-2017 Peter Powell <petpow@saberuk.com>
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

namespace CheckExemption
{
	class EventListener;
	class EventProvider;
}

class CheckExemption::EventListener
	: public Events::ModuleEventListener
{
 protected:
	EventListener(Module* mod)
		: ModuleEventListener(mod, "event/exemption")
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

class CheckExemption::EventProvider
	: public Events::ModuleEventProvider
{
 public:
	EventProvider(Module* mod)
		: ModuleEventProvider(mod, "event/exemption")
	{
	}
};
