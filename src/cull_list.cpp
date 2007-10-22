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

/* $Core: libIRCDcull_list */

#include "inspircd.h"
#include "cull_list.h"

CullItem::CullItem(User* u, std::string &r, const char* o_reason)
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

CullItem::CullItem(User* u, const char* r, const char* o_reason)
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

User* CullItem::GetUser()
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

void CullList::AddItem(User* user, std::string &reason, const char* o_reason)
{
	AddItem(user, reason.c_str(), o_reason);
}


void CullList::AddItem(User* user, const char* reason, const char* o_reason)
{
	if (exempt.find(user) == exempt.end())
	{
		CullItem item(user, reason, o_reason);
		list.push_back(item);
		exempt[user] = user;
	}
}

void CullList::MakeSilent(User* user)
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

		User *u = a->GetUser();
		user_hash::iterator iter = ServerInstance->clientlist->find(u->nick);
		std::map<User*, User*>::iterator exemptiter = exempt.find(u);
		const char* preset_reason = u->GetOperQuit();
		std::string reason = a->GetReason();
		std::string oper_reason = *preset_reason ? preset_reason : a->GetOperReason();

		if (reason.length() > MAXQUIT - 1)
			reason.resize(MAXQUIT - 1);
		if (oper_reason.length() > MAXQUIT - 1)
			oper_reason.resize(MAXQUIT - 1);

		if (u->registered != REG_ALL)
			if (ServerInstance->unregistered_count)
				ServerInstance->unregistered_count--;

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
			if (ServerInstance->Config->GetIOHook(u->GetPort()))
			{
				try
				{
					ServerInstance->Config->GetIOHook(u->GetPort())->OnRawSocketClose(u->GetFd());
				}
				catch (CoreException& modexcept)
				{
					ServerInstance->Log(DEBUG, "%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
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
				if (!a->IsSilent())
				{
					ServerInstance->SNO->WriteToSnoMask('q',"Client exiting: %s!%s@%s [%s]",u->nick,u->ident,u->host,oper_reason.c_str());
				}
			}
			else
			{
				if ((!ServerInstance->SilentULine(u->server)) && (!a->IsSilent()))
				{
					ServerInstance->SNO->WriteToSnoMask('Q',"Client exiting on server %s: %s!%s@%s [%s]",u->server,u->nick,u->ident,u->host,oper_reason.c_str());
				}
			}
			u->AddToWhoWas();
		}

		if (iter != ServerInstance->clientlist->end())
		{
			if (IS_LOCAL(u))
			{
				std::vector<User*>::iterator x = find(ServerInstance->local_users.begin(),ServerInstance->local_users.end(),u);
				if (x != ServerInstance->local_users.end())
					ServerInstance->local_users.erase(x);
			}
			ServerInstance->clientlist->erase(iter);
			delete u;
		}

		list.erase(list.begin());
		exempt.erase(exemptiter);
	}
	return n;
}

