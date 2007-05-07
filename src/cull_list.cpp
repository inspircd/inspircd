/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
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

CullItem::CullItem(userrec* u, std::string &r, const char* o_reason)
{
	this->user = u;
	this->reason = r;
	this->silent = false;
	/* Seperate oper reason not set, use the user reason */
	if (*o_reason)
		this->oper_reason = o_reason;
	else
		this->oper_reason = r;
}

CullItem::CullItem(userrec* u, const char* r, const char* o_reason)
{
	this->user = u;
	this->reason = r;
	this->silent = false;
	/* Seperate oper reason not set, use the user reason */
	if (*o_reason)
		this->oper_reason = o_reason;
	else
		this->oper_reason = r;
}

void CullItem::MakeSilent()
{
	this->silent = true;
}

bool CullItem::IsSilent()
{
	return this->silent;
}

CullItem::~CullItem()
{
}

userrec* CullItem::GetUser()
{
	return this->user;
}

std::string& CullItem::GetReason()
{
	return this->reason;
}

std::string& CullItem::GetOperReason()
{
	return this->oper_reason;
}

CullList::CullList(InspIRCd* Instance) : ServerInstance(Instance)
{
	list.clear();
	exempt.clear();
}

void CullList::AddItem(userrec* user, std::string &reason, const char* o_reason)
{
	AddItem(user, reason.c_str(), o_reason);
}


void CullList::AddItem(userrec* user, const char* reason, const char* o_reason)
{
	if (exempt.find(user) == exempt.end())
	{
		CullItem item(user, reason, o_reason);
		list.push_back(item);
		exempt[user] = user;
	}
}

void CullList::MakeSilent(userrec* user)
{
	for (std::vector<CullItem>::iterator a = list.begin(); a != list.end(); ++a)
	{
		if (a->GetUser() == user)
		{
			a->MakeSilent();
			break;
		}
	}
	return;
}

int CullList::Apply()
{
	int n = list.size();
	while (list.size())
	{
		std::vector<CullItem>::iterator a = list.begin();

		user_hash::iterator iter = ServerInstance->clientlist->find(a->GetUser()->nick);
		std::map<userrec*, userrec*>::iterator exemptiter = exempt.find(a->GetUser());
		const char* preset_reason = a->GetUser()->GetOperQuit();
		std::string reason = a->GetReason();
		std::string oper_reason = *preset_reason ? preset_reason : a->GetOperReason();

		if (reason.length() > MAXQUIT - 1)
			reason.resize(MAXQUIT - 1);
		if (oper_reason.length() > MAXQUIT - 1)
			oper_reason.resize(MAXQUIT - 1);

		if (a->GetUser()->registered != REG_ALL)
			if (ServerInstance->unregistered_count)
				ServerInstance->unregistered_count--;

		if (IS_LOCAL(a->GetUser()))
		{
			a->GetUser()->Write("ERROR :Closing link (%s@%s) [%s]", a->GetUser()->ident, a->GetUser()->host, oper_reason.c_str());
			if ((!a->GetUser()->sendq.empty()) && (!(*a->GetUser()->GetWriteError())))
				a->GetUser()->FlushWriteBuf();
		}

		if (a->GetUser()->registered == REG_ALL)
		{
			FOREACH_MOD_I(ServerInstance,I_OnUserQuit,OnUserQuit(a->GetUser(), reason, oper_reason));
			a->GetUser()->PurgeEmptyChannels();
			a->GetUser()->WriteCommonQuit(reason, oper_reason);
		}

		FOREACH_MOD_I(ServerInstance,I_OnUserDisconnect,OnUserDisconnect(a->GetUser()));

		if (IS_LOCAL(a->GetUser()))
		{
			if (ServerInstance->Config->GetIOHook(a->GetUser()->GetPort()))
			{
				try
				{
					ServerInstance->Config->GetIOHook(a->GetUser()->GetPort())->OnRawSocketClose(a->GetUser()->GetFd());
				}
				catch (CoreException& modexcept)
				{
					ServerInstance->Log(DEBUG, "%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
				}
			}

			ServerInstance->SE->DelFd(a->GetUser());
			a->GetUser()->CloseSocket();
		}

		/*
		 * this must come before the ServerInstance->SNO->WriteToSnoMaskso that it doesnt try to fill their buffer with anything
		 * if they were an oper with +sn +qQ.
		 */
		if (a->GetUser()->registered == REG_ALL)
		{
			if (IS_LOCAL(a->GetUser()))
			{
				if (!a->IsSilent())
				{
					ServerInstance->SNO->WriteToSnoMask('q',"Client exiting: %s!%s@%s [%s]",a->GetUser()->nick,a->GetUser()->ident,a->GetUser()->host,oper_reason.c_str());
				}
			}
			else
			{
				if ((!ServerInstance->SilentULine(a->GetUser()->server)) && (!a->IsSilent()))
				{
					ServerInstance->SNO->WriteToSnoMask('Q',"Client exiting on server %s: %s!%s@%s [%s]",a->GetUser()->server,a->GetUser()->nick,a->GetUser()->ident,a->GetUser()->host,oper_reason.c_str());
				}
			}
			a->GetUser()->AddToWhoWas();
		}

		if (iter != ServerInstance->clientlist->end())
		{
			if (IS_LOCAL(a->GetUser()))
			{
				std::vector<userrec*>::iterator x = find(ServerInstance->local_users.begin(),ServerInstance->local_users.end(),a->GetUser());
				if (x != ServerInstance->local_users.end())
					ServerInstance->local_users.erase(x);
			}
			ServerInstance->clientlist->erase(iter);
			DELETE(a->GetUser());
		}

		list.erase(list.begin());
		exempt.erase(exemptiter);
	}
	return n;
}

