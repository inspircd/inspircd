/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2016 Attila Molnar <attilamolnar@hush.com>
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

CmdResult CommandNum::HandleServer(TreeServer* server, std::vector<std::string>& params)
{
	User* const target = ServerInstance->FindUUID(params[1]);
	if (!target)
		return CMD_FAILURE;

	LocalUser* const localtarget = IS_LOCAL(target);
	if (!localtarget)
		return CMD_SUCCESS;

	Numeric::Numeric numeric(ConvToInt(params[2]));
	// Passing NULL is ok, in that case the numeric source becomes this server
	numeric.SetServer(Utils->FindServerID(params[0]));
	numeric.GetParams().insert(numeric.GetParams().end(), params.begin()+3, params.end());

	localtarget->WriteNumeric(numeric);
	return CMD_SUCCESS;
}

RouteDescriptor CommandNum::GetRouting(User* user, const std::vector<std::string>& params)
{
	return ROUTE_UNICAST(params[1]);
}

CommandNum::Builder::Builder(SpanningTree::RemoteUser* target, const Numeric::Numeric& numeric)
	: CmdBuilder("NUM")
{
	TreeServer* const server = (numeric.GetServer() ? (static_cast<TreeServer*>(numeric.GetServer())) : Utils->TreeRoot);
	push(server->GetID()).push(target->uuid).push(InspIRCd::Format("%03u", numeric.GetNumeric()));
	const std::vector<std::string>& params = numeric.GetParams();
	if (!params.empty())
	{
		for (std::vector<std::string>::const_iterator i = params.begin(); i != params.end()-1; ++i)
			push(*i);
		push_last(params.back());
	}
}
