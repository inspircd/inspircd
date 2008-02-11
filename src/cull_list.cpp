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

#include "inspircd.h"
#include "users.h"
#include "cull_list.h"

CullList::CullList(InspIRCd* Instance) : ServerInstance(Instance)
{
	list.clear();
}

void CullList::AddItem(userrec* user, std::string &reason, const char* o_reason)
{
	AddItem(user, reason.c_str(), o_reason);
}


void CullList::AddItem(userrec* user, const char* reason, const char* o_reason)
{
	if (user->quitting)
		return;
		
	user->quitting = true;
	user->quitmsg = reason;
	user->operquitmsg = o_reason;

	list.push_back(user);
}

void CullList::MakeSilent(userrec* user)
{
	user->silentquit = true;
}

int CullList::Apply()
{
	int i = 0;
	int n = list.size();
	
	while (list.size() && i++ != 100)
	{
		std::vector<userrec *>::iterator li = list.begin();
		userrec *a = (*li);
		
		user_hash::iterator iter = ServerInstance->clientlist->find(a->nick);
		const char* preset_reason = a->operquitmsg.c_str();
		std::string reason = a->quitmsg;
		std::string oper_reason = *preset_reason ? preset_reason : a->operquitmsg;

		if (reason.length() > MAXQUIT - 1)
			reason.resize(MAXQUIT - 1);
		if (oper_reason.length() > MAXQUIT - 1)
			oper_reason.resize(MAXQUIT - 1);

		if (a->registered != REG_ALL)
			if (ServerInstance->unregistered_count)
				ServerInstance->unregistered_count--;

		if (IS_LOCAL(a))
		{
			if ((!a->sendq.empty()) && (!(*a->GetWriteError())))
				a->FlushWriteBuf();
		}

		if (a->registered == REG_ALL)
		{
			FOREACH_MOD_I(ServerInstance,I_OnUserQuit,OnUserQuit(a, reason, oper_reason));
			a->PurgeEmptyChannels();
			a->WriteCommonQuit(reason, oper_reason);
		}

		FOREACH_MOD_I(ServerInstance,I_OnUserDisconnect,OnUserDisconnect(a));

		if (IS_LOCAL(a))
		{
			if (ServerInstance->Config->GetIOHook(a->GetPort()))
			{
				try
				{
					ServerInstance->Config->GetIOHook(a->GetPort())->OnRawSocketClose(a->GetFd());
				}
				catch (CoreException& modexcept)
				{
					ServerInstance->Log(DEBUG, "%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
				}
			}

			ServerInstance->SE->DelFd(a);
			a->CloseSocket();
		}

		/*
		 * this must come before the ServerInstance->SNO->WriteToSnoMaskso that it doesnt try to fill their buffer with anything
		 * if they were an oper with +sn +qQ.
		 */
		if (a->registered == REG_ALL)
		{
			if (IS_LOCAL(a))
			{
				if (!a->silentquit)
				{
					ServerInstance->SNO->WriteToSnoMask('q',"Client exiting: %s!%s@%s [%s]",a->nick,a->ident,a->host,oper_reason.c_str());
				}
			}
			else
			{
				if ((!ServerInstance->SilentULine(a->server)) && (!a->silentquit))
				{
					ServerInstance->SNO->WriteToSnoMask('Q',"Client exiting on server %s: %s!%s@%s [%s]",a->server,a->nick,a->ident,a->host,oper_reason.c_str());
				}
			}
			a->AddToWhoWas();
		}

		if (iter != ServerInstance->clientlist->end())
		{
			if (IS_LOCAL(a))
			{
				std::vector<userrec*>::iterator x = find(ServerInstance->local_users.begin(),ServerInstance->local_users.end(),a);
				if (x != ServerInstance->local_users.end())
					ServerInstance->local_users.erase(x);
			}
			ServerInstance->clientlist->erase(iter);
			delete a;
		}

		list.erase(list.begin());
	}
	return n;
}

