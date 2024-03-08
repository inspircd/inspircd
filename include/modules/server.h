/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2017-2019, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
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

namespace ServerProtocol
{
	class LinkEventListener;
	class MessageEventListener;
	class RouteEventListener;
	class SyncEventListener;
}

enum
{
	// From ircu.
	RPL_MAP = 15,
	RPL_ENDMAP = 17,

	// InspIRCd-specific.
	RPL_MAPUSERS = 18,
};

class ServerProtocol::LinkEventListener
	: public Events::ModuleEventListener
{
public:
	LinkEventListener(Module* mod, unsigned int eventprio = DefaultPriority)
		: ModuleEventListener(mod, "event/server-link", eventprio)
	{
	}

	/** Fired when a server has linked to the network.
	 * @param server Server that recently linked.
	 */
	virtual void OnServerLink(const Server* server) { }

	/** Fired when a server has finished bursting.
	 * @param server Server that recently finished bursting.
	 */
	virtual void OnServerBurst(const Server* server) { }

	/** Fired when a server splits
	 * @param server Server that split
	 * @param error Whether the server split because of an error.
	 */
	virtual void OnServerSplit(const Server* server, bool error) { }
};

class ServerProtocol::MessageEventListener
	: public Events::ModuleEventListener
{
public:
	MessageEventListener(Module* mod, unsigned int eventprio = DefaultPriority)
		: ModuleEventListener(mod, "event/server-message", eventprio)
	{
	}

	/** Fired when a server message is being sent by a user.
	 * @param source The user who sent the message.
	 * @param name The name of the command which was sent.
	 * @param tags The tags which will be sent with the message.
	 */
	virtual void OnBuildUserMessage(const User* source, const char* name, ClientProtocol::TagMap& tags) { }

	/** Fired when a server message is being sent by a server.
	 * @param source The server who sent the message.
	 * @param name The name of the command which was sent.
	 * @param tags The tags which will be sent with the message.
	 */
	virtual void OnBuildServerMessage(const Server* source, const char* name, ClientProtocol::TagMap& tags) { }
};

class ServerProtocol::RouteEventListener
	: public Events::ModuleEventListener
{
public:
	RouteEventListener(Module* mod, unsigned int eventprio = DefaultPriority)
		: ModuleEventListener(mod, "event/server-route", eventprio)
	{
	}

	/** Fired when a channel message is being routed across the network.
	 * @param channel The channel which is having a message sent to it.
	 * @param server The server which might have a message routed to it.
	 * @return Either MOD_RES_ALLOW to always send the message to the server, MOD_RES_DENY to never
	 *         send the message to the server, or MOD_RES_PASSTHRU to not handle the event.
	 */
	virtual ModResult OnRouteMessage(const Channel* channel, const Server* server) { return MOD_RES_PASSTHRU; }
};


class ServerProtocol::SyncEventListener
	: public Events::ModuleEventListener
{
public:
	SyncEventListener(Module* mod, unsigned int eventprio = DefaultPriority)
		: ModuleEventListener(mod, "event/server-sync", eventprio)
	{
	}

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
