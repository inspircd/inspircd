/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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
#include "core_oper.h"

ModeUserOperator::ModeUserOperator(Module* Creator)
	: SimpleUserMode(Creator, "oper", 'o', true)
{
}

bool ModeUserOperator::OnModeChange(User* source, User* dest, Channel*, Modes::Change& change)
{
	// Only services pseudoclients and server operators can log out a
	// server operator.
	if (!source->server->IsService() && !source->IsOper())
		return false;

	// The setting of the oper mode is done in User::OperLogin without
	// calling the handler so reject any attempt to manually set it.
	if (change.adding)
		return false;

	// Notify server operators of the logout.
	char snomask = IS_LOCAL(dest) ? 'o' : 'O';
	ServerInstance->SNO.WriteToSnoMask(snomask, "{} ({}) [{}] logged {}{}out of their server operator account.",
		source->nick, source->GetRealUserHost(), source->GetAddress(),
		source == dest ? "" : dest->nick, source == dest ? "" : " ");

	// Log the server operator out of their account.
	dest->OperLogout();
	return true;
}
