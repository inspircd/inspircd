/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017, 2023 Sadie Powell <sadie@witchery.services>
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

namespace WebIRC
{
	class EventListener;

	using FlagMap = insp::casemapped_flat_map<std::string>;
}

class WebIRC::EventListener
	: public Events::ModuleEventListener
{
protected:
	EventListener(Module* mod, unsigned int eventprio = DefaultPriority)
		: ModuleEventListener(mod, "webirc", eventprio)
	{
	}

public:
	virtual void OnWebIRCAuth(LocalUser* user, const FlagMap* flags) = 0;
};
