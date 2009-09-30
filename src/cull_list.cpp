/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
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

void CullList::MakeSilent(User* user)
{
	user->quietquit = true;
	return;
}

void CullList::Apply()
{
	for(std::vector<User *>::iterator a = list.begin(); a != list.end(); a++)
	{
		User *u = *a;
		// user has been moved onto their UID; that's why this isn't find(u->nick)
		user_hash::iterator iter = ServerInstance->Users->clientlist->find(u->uuid);

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

		if (iter != ServerInstance->Users->clientlist->end())
		{
			ServerInstance->Users->clientlist->erase(iter);
		}
		else
		{
			ServerInstance->Logs->Log("CULLLIST", DEBUG, "iter == clientlist->end, can't remove them from hash... problematic..");
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

