/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

class ServerEventListener : public Events::ModuleEventListener
{
 public:
	ServerEventListener(Module* mod)
		: ModuleEventListener(mod, "event/server")
	{
	}

	/** Fired when a server finishes burst
	 * @param server Server that recently linked and finished burst
	 */
	virtual void OnServerLink(const Server* server) { }

	 /** Fired when a server splits
	  * @param server Server that split
	  */
	virtual void OnServerSplit(const Server* server) { }

	/** Allows modules to synchronize user metadata during a netburst. This will
	 * be called for every user visible on your side of the burst.
	 * @param user The user being synchronized.
	 * @param server The target of the burst.
	 */
	virtual void OnSyncUser(User* user, ProtocolServer& server) { }

	/** Allows modules to synchronize channel metadata during a netburst. This will
	 * be called for every channel visible on your side of the burst.
	 * @param chan The channel being synchronized.
	 * @param server The target of the burst.
	 */
	virtual void OnSyncChannel(Channel* chan, ProtocolServer& server) { }

	/** Allows modules to synchronize network metadata during a netburst.
	 * @param server The target of the burst.
	 */
	virtual void OnSyncNetwork(ProtocolServer& server) { }
	
};
