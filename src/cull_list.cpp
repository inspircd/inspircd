/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core: libIRCDcull_list */

#include "inspircd.h"
#include "cull_list.h"

CullList::CullList(InspIRCd* Instance) : ServerInstance(Instance)
{
	list.clear();
}

void CullList::AddItem(User* user)
{
	if (user->quitting)
	{
		ServerInstance->Logs->Log("CULLLIST",DEBUG, "*** Warning *** - You tried to quit a user (%s) twice. Did your module call QuitUser twice?", user->nick.c_str());
		return;
	}

	user->quitting = true;
	list.push_back(user);
}

void CullList::MakeSilent(User* user)
{
	user->quietquit = true;
	return;
}

int CullList::Apply()
{
	int n = list.size();
	int i = 0;

	while (list.size() && i++ != 100)
	{
		std::vector<User *>::iterator a = list.begin();

		User *u = (*a);
		user_hash::iterator iter = ServerInstance->Users->clientlist->find(u->nick);
		const std::string& preset_reason = u->GetOperQuit();
		std::string reason;
		std::string oper_reason;

		reason.assign(u->quitmsg, 0, MAXQUIT - 1);
		oper_reason.assign(preset_reason.empty() ? preset_reason : u->operquitmsg, 0, MAXQUIT - 1);

		if (u->registered != REG_ALL)
			if (ServerInstance->Users->unregistered_count)
				ServerInstance->Users->unregistered_count--;

		if (IS_LOCAL(u))
		{
			if ((!u->sendq.empty()) && (!(*u->GetWriteError())))
				u->FlushWriteBuf();
		}

		if (u->registered == REG_ALL)
		{
			FOREACH_MOD_I(ServerInstance,I_OnUserQuit,OnUserQuit(u, reason, oper_reason));
			u->PurgeEmptyChannels();
			u->WriteCommonQuit(reason, oper_reason);
		}

		FOREACH_MOD_I(ServerInstance,I_OnUserDisconnect,OnUserDisconnect(u));

		if (IS_LOCAL(u))
		{
			if (u->io)
			{
				try
				{
					u->io->OnRawSocketClose(u->GetFd());
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
			if (IS_LOCAL(u))
			{
				if (!u->quietquit)
				{
					ServerInstance->SNO->WriteToSnoMask('q',"Client exiting: %s!%s@%s [%s]", u->nick.c_str(), u->ident.c_str(), u->host.c_str(), oper_reason.c_str());
				}
			}
			else
			{
				if ((!ServerInstance->SilentULine(u->server)) && (!u->quietquit))
				{
					ServerInstance->SNO->WriteToSnoMask('Q',"Client exiting on server %s: %s!%s@%s [%s]", u->server, u->nick.c_str(), u->ident.c_str(), u->host.c_str(), oper_reason.c_str());
				}
			}
			u->AddToWhoWas();
		}

		bool deleteu = true;

		if (iter != ServerInstance->Users->clientlist->end())
		{
			ServerInstance->Users->clientlist->erase(iter);
		}
		else
		{
			/*
			 * Trying to track down Jason's issue.. this should never happen obviously.
			 */
			ServerInstance->Logs->Log("CULLLIST", DEBUG, "iter == clientlist->end, can't remove them from hash... problematic?");
//			deleteu = false;
// actually, delete them anyway.. the local vector is the real problem here
		}

		if (IS_LOCAL(u))
		{
			std::vector<User*>::iterator x = find(ServerInstance->Users->local_users.begin(),ServerInstance->Users->local_users.end(),u);
			if (x != ServerInstance->Users->local_users.end())
				ServerInstance->Users->local_users.erase(x);
			else
			{
				/*
				 * This code is in here to monitor an issue of Jason's, where it seems to be trying to quit already quit users.
				 * The only way that can happen is if this find fails, so log it just in case.
				 * Also, (perhaps incorrectly, but oh well), return here so we don't delete the user and then start trampling
				 * on deleted memory, which leads to big problems..
				 */
				ServerInstance->Logs->Log("CULLLIST", DEBUG, "Failed to remove user from vector, we're all gonna die!!! Not deleting the user to save our sanity");
				deleteu = false;
			}
		}

		if (deleteu)
		{
			delete u;
		}

		list.erase(list.begin());
	}

	return n;
}

