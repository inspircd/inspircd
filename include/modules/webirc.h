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

namespace WebIRC
{
	class EventListener;

	typedef insp::flat_map<std::string, std::string, irc::insensitive_swo> FlagMap;
}

class WebIRC::EventListener
	: public Events::ModuleEventListener
{
 protected:
	EventListener(Module* mod)
		: ModuleEventListener(mod, "event/webirc")
	{
	}

 public:
	virtual void OnWebIRCAuth(LocalUser* user, const FlagMap* flags) = 0;
};
