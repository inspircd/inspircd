/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018, 2020-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
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
#include "timeutils.h"
#include "xline.h"

#include "treeserver.h"
#include "utils.h"
#include "commands.h"

CmdResult CommandAddLine::Handle(User* usr, Params& params)
{
	XLineFactory* xlf = ServerInstance->XLines->GetFactory(params[0]);
	const std::string& setter = usr->nick;

	if (!xlf)
	{
		ServerInstance->SNO.WriteToSnoMask('x', "{} sent me an unknown ADDLINE type ({}).", setter, params[0]);
		return CmdResult::FAILURE;
	}

	XLine* xl = nullptr;
	try
	{
		xl = xlf->Generate(ServerInstance->Time(), ConvToNum<unsigned long>(params[4]), params[2], params[5], params[1]);
	}
	catch (const ModuleException& e)
	{
		ServerInstance->SNO.WriteToSnoMask('x', "Unable to ADDLINE type {} from {}: {}", params[0], setter, e.GetReason());
		return CmdResult::FAILURE;
	}
	xl->SetCreateTime(ServerCommand::ExtractTS(params[3]));
	if (ServerInstance->XLines->AddLine(xl, nullptr))
	{
		if (xl->duration)
		{
			ServerInstance->SNO.WriteToSnoMask('X', "{} added a timed {}{} on {}, expires in {} (on {}): {}",
				setter, params[0], params[0].length() <= 2 ? "-line" : "",
				params[1], Duration::ToString(xl->duration),
				Time::ToString(xl->expiry), params[5]);
		}
		else
		{
			ServerInstance->SNO.WriteToSnoMask('X', "{} added a permanent {}{} on {}: {}",
				setter, params[0], params[0].length() <= 2 ? "-line" : "",
				params[1], params[5]);
		}

		TreeServer* remoteserver = TreeServer::Get(usr);

		if (!remoteserver->IsBursting())
		{
			ServerInstance->XLines->ApplyLines();
		}
		return CmdResult::SUCCESS;
	}
	else
	{
		delete xl;
		return CmdResult::FAILURE;
	}
}

CommandAddLine::Builder::Builder(XLine* xline, User* user)
	: CmdBuilder(user, "ADDLINE")
{
	push(xline->type);
	push(xline->Displayable());
	push(xline->source);
	push_int(xline->set_time);
	push_int(xline->duration);
	push_last(xline->reason);
}
