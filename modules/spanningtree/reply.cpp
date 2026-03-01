/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2026 Sadie Powell <sadie@witchery.services>
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

#include "utils.h"
#include "commands.h"
#include "remoteuser.h"

CmdResult CommandReply::HandleServer(TreeServer* server, CommandBase::Params& params)
{
	// <source-sid> <target-uuid> <command> <code> [<params>] :<message>
	auto* target = ServerInstance->Users.FindUUID(params[1]);
	if (!target)
		return CmdResult::FAILURE; // User has gone.

	auto* ltarget = IS_LOCAL(target);
	if (!ltarget)
		return CmdResult::SUCCESS; // Not for us.

	Command* cmd = nullptr;
	if (params[2] != "*")
		cmd = ServerInstance->Parser.GetHandler(params[2]);

	Reply::Reply reply(this->type, cmd, params[3]);
	reply.SetSource(Utils->FindServerID(params[0]));
	reply.GetParams().insert(reply.GetParams().end(), params.begin() + 4, params.end());

	ltarget->WriteReply(reply);
	return CmdResult::SUCCESS;
}

RouteDescriptor CommandReply::GetRouting(User* user, const Params& params)
{
	return ROUTE_UNICAST(params[1]);
}

CommandReply::Builder::Builder(SpanningTree::RemoteUser* target, const Reply::Reply& reply)
	: CmdBuilder(Reply::CommandStrFromType(reply.GetType()))
{
	push(reply.GetSource() ? reply.GetSource()->GetId() : Utils->TreeRoot->GetId());
	push(target->uuid);
	push(reply.GetCommand() ? reply.GetCommand()->name : "*");
	push(reply.GetCode());

	const auto& params = reply.GetParams();
	if (!params.empty())
	{
		for (auto it = params.begin(); it != params.end() - 1; ++it)
			push(*it);
		push_last(params.back());
	}
	push_tags(params.GetTags());
}
