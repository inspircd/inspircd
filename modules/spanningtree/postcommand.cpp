/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <brain@inspircd.org>
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
#include "translate.h"
#include "treeserver.h"

void ModuleSpanningTree::OnPostCommand(Command* command, const CommandBase::Params& parameters, LocalUser* user, CmdResult result, bool loop)
{
	if (result == CmdResult::SUCCESS)
		Utils->RouteCommand(nullptr, command, parameters, user);
}

void SpanningTreeUtilities::RouteCommand(TreeServer* origin, CommandBase* thiscmd, const CommandBase::Params& parameters, User* user)
{
	RouteDescriptor routing = thiscmd->GetRouting(user, parameters);
	if (routing.type == RouteType::LOCAL)
		return;

	const std::string& command = thiscmd->service_name;
	const bool encap = ((routing.type == RouteType::OPTIONAL_BROADCAST) || (routing.type == RouteType::OPTIONAL_UNICAST));
	MessageBuilder msg(user, encap ? "ENCAP" : command);
	msg.PushTags(parameters.GetTags());
	const TreeServer* sdest = nullptr;

	if (routing.type == RouteType::OPTIONAL_BROADCAST)
	{
		msg.Push('*', command);
	}
	else if (routing.type == RouteType::UNICAST || routing.type == RouteType::OPTIONAL_UNICAST)
	{
		sdest = static_cast<const TreeServer*>(routing.server);
		if (!sdest)
		{
			// Assume the command handler already validated routing.target and have only returned success if the target is something that the
			// user executing the command is allowed to look up e.g. target is not an uuid if user is local.
			sdest = FindRouteTarget(routing.target);
			if (!sdest)
			{
				ServerInstance->Logs.Debug(MODNAME, "Trying to route {}{} to nonexistent server {}", (encap ? "ENCAP " : ""), command, routing.target);
				return;
			}
		}

		if (encap)
			msg.Push(sdest->GetId(), command);
	}
	else
	{
		// The lock here should always succeed because we were called by routing
		// a command from it.
		const auto& srcmodule = thiscmd->service_creator.lock();
		if (!(srcmodule->properties & (VF_COMMON | VF_CORE)) && !insp::same_ptr(srcmodule, CreatorPtr))
		{
			ServerInstance->Logs.Normal(MODNAME, "Routed command {} from non-VF_COMMON module {}",
				command, srcmodule->ModuleFile);
			return;
		}
	}

	msg.PushParams(Translate::ParamsToNetwork(thiscmd->translation, parameters, thiscmd));

	if (routing.type == RouteType::MESSAGE)
	{
		char pfx = 0;
		std::string dest = routing.target;
		if (ServerInstance->Modes.FindPrefix(dest[0]))
		{
			pfx = dest[0];
			dest.erase(dest.begin());
		}
		if (ServerInstance->Channels.IsPrefix(dest[0]))
		{
			auto* c = ServerInstance->Channels.Find(dest);
			if (!c)
				return;
			// TODO OnBuildExemptList hook was here
			User::List exempts;
			std::string message;
			if (parameters.size() >= 2)
				message.assign(parameters[1]);
			SendChannelMessage(user, c, message, pfx, parameters.GetTags(), exempts, command.c_str(), origin ? origin->GetSocket() : nullptr);
		}
		else if (dest[0] == '$')
		{
			msg.Broadcast(origin);
		}
		else
		{
			// user target?
			auto* d = ServerInstance->Users.Find(dest);
			if (!d || d->IsLocal())
				return;
			TreeServer* tsd = TreeServer::Get(d)->GetRoute();
			if (tsd == origin)
			{
				// huh? no routing stuff around in a circle, please.
				return;
			}
			msg.Unicast(d);
		}
	}
	else if (routing.type == RouteType::BROADCAST || routing.type == RouteType::OPTIONAL_BROADCAST)
	{
		msg.Broadcast(origin);
	}
	else if (routing.type == RouteType::UNICAST || routing.type == RouteType::OPTIONAL_UNICAST)
	{
		msg.Unicast(sdest->ServerUser);
	}
}
