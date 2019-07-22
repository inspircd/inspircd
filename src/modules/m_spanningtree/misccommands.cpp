/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2007-2008, 2012 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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
#include "commands.h"
#include "treeserver.h"

void CmdBuilder::FireEvent(Server* target, const char* cmd, ClientProtocol::TagMap& taglist)
{
	FOREACH_MOD_CUSTOM(Utils->Creator->GetMessageEventProvider(), ServerProtocol::MessageEventListener, OnBuildMessage, (target, cmd, taglist));
	UpdateTags();
}

void CmdBuilder::FireEvent(User* target, const char* cmd, ClientProtocol::TagMap& taglist)
{
	FOREACH_MOD_CUSTOM(Utils->Creator->GetMessageEventProvider(), ServerProtocol::MessageEventListener, OnBuildMessage, (target, cmd, taglist));
	UpdateTags();
}

void CmdBuilder::UpdateTags()
{
	std::string taglist;
	if (!tags.empty())
	{
		char separator = '@';
		for (ClientProtocol::TagMap::const_iterator iter = tags.begin(); iter != tags.end(); ++iter)
		{
			taglist.push_back(separator);
			separator = ';';
			taglist.append(iter->first);
			if (!iter->second.value.empty())
			{
				taglist.push_back('=');
				taglist.append(iter->second.value);
			}
		}
		taglist.push_back(' ');
	}
	content.replace(0, tagsize, taglist);
	tagsize = taglist.length();
}

CmdResult CommandSNONotice::Handle(User* user, Params& params)
{
	ServerInstance->SNO->WriteToSnoMask(params[0][0], "From " + user->nick + ": " + params[1]);
	return CMD_SUCCESS;
}

CmdResult CommandEndBurst::HandleServer(TreeServer* server, Params& params)
{
	server->FinishBurst();
	return CMD_SUCCESS;
}
