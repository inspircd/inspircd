/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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

class CommandWallops final
	: public Command
{
	SimpleUserMode wallopsmode;
	ClientProtocol::EventProvider protoevprov;

public:
	CommandWallops(Module* parent)
		: Command(parent, "WALLOPS", 1, 1)
		, wallopsmode(parent, "wallops", 'w')
		, protoevprov(parent, name)
	{
		access_needed = CmdAccess::OPERATOR;
		allow_empty_last_param = true;
		syntax = { ":<message>" };
	}

	CmdResult Handle(User* user, const Params& parameters) override;

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_BROADCAST;
	}
};

CmdResult CommandWallops::Handle(User* user, const Params& parameters)
{
	if (parameters[0].empty())
	{
		user->WriteNumeric(ERR_NOTEXTTOSEND, "No text to send");
		return CmdResult::FAILURE;
	}

	ClientProtocol::Message msg("WALLOPS", user);
	msg.PushParamRef(parameters[0]);
	ClientProtocol::Event wallopsevent(protoevprov, msg);

	for (auto* curr : ServerInstance->Users.GetLocalUsers())
	{
		if (curr->IsModeSet(wallopsmode))
			curr->Send(wallopsevent);
	}

	return CmdResult::SUCCESS;
}

class CoreModWallops final
	: public Module
{
private:
	CommandWallops cmd;

public:
	CoreModWallops()
		: Module(VF_CORE | VF_VENDOR, "Provides the WALLOPS command")
		, cmd(this)
	{
	}
};

MODULE_INIT(CoreModWallops)
