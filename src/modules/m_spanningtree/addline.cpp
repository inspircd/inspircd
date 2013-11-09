/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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
#include "xline.h"

#include "treesocket.h"
#include "treeserver.h"
#include "utils.h"

/* $ModDep: m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */

bool TreeSocket::AddLine(const std::string &prefix, parameterlist &params)
{
	if (params.size() < 6)
	{
		std::string servername = MyRoot->GetName();
		ServerInstance->SNO->WriteToSnoMask('d', "%s sent me a malformed ADDLINE", servername.c_str());
		return true;
	}

	XLineFactory* xlf = ServerInstance->XLines->GetFactory(params[0]);

	std::string setter = "<unknown>";
	User* usr = ServerInstance->FindNick(prefix);
	if (usr)
		setter = usr->nick;
	else
	{
		TreeServer* t = Utils->FindServer(prefix);
		if (t)
			setter = t->GetName();
	}

	if (!xlf)
	{
		ServerInstance->SNO->WriteToSnoMask('d',"%s sent me an unknown ADDLINE type (%s).",setter.c_str(),params[0].c_str());
		return true;
	}

	long created = atol(params[3].c_str()), expires = atol(params[4].c_str());
	if (created < 0 || expires < 0)
		return true;

	XLine* xl = NULL;
	try
	{
		xl = xlf->Generate(ServerInstance->Time(), expires, params[2], params[5], params[1]);
	}
	catch (ModuleException &e)
	{
		ServerInstance->SNO->WriteToSnoMask('d',"Unable to ADDLINE type %s from %s: %s", params[0].c_str(), setter.c_str(), e.GetReason());
		return true;
	}
	xl->SetCreateTime(created);
	if (ServerInstance->XLines->AddLine(xl, NULL))
	{
		if (xl->duration)
		{
			std::string timestr = ServerInstance->TimeString(xl->expiry);
			ServerInstance->SNO->WriteToSnoMask('X',"%s added %s%s on %s to expire on %s: %s",setter.c_str(),params[0].c_str(),params[0].length() == 1 ? "-line" : "",
					params[1].c_str(), timestr.c_str(), params[5].c_str());
		}
		else
		{
			ServerInstance->SNO->WriteToSnoMask('X',"%s added permanent %s%s on %s: %s",setter.c_str(),params[0].c_str(),params[0].length() == 1 ? "-line" : "",
					params[1].c_str(),params[5].c_str());
		}
		params[5] = ":" + params[5];

		User* u = ServerInstance->FindNick(prefix);
		Utils->DoOneToAllButSender(prefix, "ADDLINE", params, u ? u->server : prefix);
		TreeServer *remoteserver = Utils->FindServer(u ? u->server : prefix);

		if (!remoteserver->bursting)
		{
			ServerInstance->XLines->ApplyLines();
		}
	}
	else
		delete xl;

	return true;
}

