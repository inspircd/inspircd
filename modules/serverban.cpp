/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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
#include "modules/extban.h"

class ServerExtBan final
	: public ExtBan::MatchingBase
{
public:
	ServerExtBan(Module* Creator)
		: ExtBan::MatchingBase(Creator, "server", 's')
	{
	}

	bool IsMatch(User* user, Channel* channel, const std::string& text) override
	{
		return InspIRCd::Match(user->server->GetPublicName(), text);
	}
};

class ModuleServerBan final
	: public Module
{
private:
	ServerExtBan extban;

public:
	ModuleServerBan()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds extended ban s: (server) which check whether users are on a server matching the specified glob pattern.")
		, extban(this)
	{
	}
};

MODULE_INIT(ModuleServerBan)
