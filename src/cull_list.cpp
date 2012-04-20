/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2006-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2007 Craig Edwards <craigedwards@brainbox.cc>
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


/* $Core */

#include "inspircd.h"
#include "cull_list.h"

CullList::CullList(InspIRCd* Instance) : ServerInstance(Instance)
{
}

void CullList::AddItem(User* user)
{
	list.push_back(user);
}

void CullList::AddSQItem(User* user)
{
	SQlist.push_back(user);
}

void CullList::MakeSilent(User* user)
{
	user->quietquit = true;
	return;
}

void CullList::Apply()
{
	std::vector<User *> working;
	while (!SQlist.empty())
	{
		working.swap(SQlist);
		for(std::vector<User *>::iterator a = working.begin(); a != working.end(); a++)
		{
			User *u = *a;
			ServerInstance->SNO->WriteToSnoMask('a', "User %s SendQ exceeds connect class maximum of %lu",
				u->nick.c_str(), u->MyClass->GetSendqMax());
			ServerInstance->Users->QuitUser(u, "SendQ exceeded");
		}
		working.clear();
	}
	for(std::vector<User *>::iterator a = list.begin(); a != list.end(); a++)
	{
		User *u = *a;

		if (u->registered != REG_ALL)
			if (ServerInstance->Users->unregistered_count)
				ServerInstance->Users->unregistered_count--;

		if (IS_LOCAL(u))
		{
			if (!u->sendq.empty())
				u->FlushWriteBuf();

			if (u->GetIOHook())
			{
				try
				{
					u->GetIOHook()->OnRawSocketClose(u->GetFd());
				}
				catch (CoreException& modexcept)
				{
					ServerInstance->Logs->Log("CULLLIST",DEBUG, "%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
				}
			}

			ServerInstance->SE->DelFd(u);
			u->CloseSocket();
		}

		/*
		 * this must come before the ServerInstance->SNO->WriteToSnoMaskso that it doesnt try to fill their buffer with anything
		 * if they were an oper with +sn +qQ.
		 */
		if (u->registered == REG_ALL)
		{
			u->PurgeEmptyChannels();
			if (IS_LOCAL(u))
			{
				if (!u->quietquit)
				{
					ServerInstance->SNO->WriteToSnoMask('q',"Client exiting: %s!%s@%s [%s]", u->nick.c_str(), u->ident.c_str(), u->host.c_str(), u->operquitmsg.c_str());
				}
			}
			else
			{
				if ((!ServerInstance->SilentULine(u->server)) && (!u->quietquit))
				{
					ServerInstance->SNO->WriteToSnoMask('Q',"Client exiting on server %s: %s!%s@%s [%s]", u->server, u->nick.c_str(), u->ident.c_str(), u->host.c_str(), u->operquitmsg.c_str());
				}
			}
			u->AddToWhoWas();
		}

		if (IS_LOCAL(u))
		{
			std::vector<User*>::iterator x = find(ServerInstance->Users->local_users.begin(),ServerInstance->Users->local_users.end(),u);
			if (x != ServerInstance->Users->local_users.end())
				ServerInstance->Users->local_users.erase(x);
			else
			{
				ServerInstance->Logs->Log("CULLLIST", DEBUG, "Failed to remove user from vector..");
			}
		}

		delete u;
	}
	list.clear();
}

