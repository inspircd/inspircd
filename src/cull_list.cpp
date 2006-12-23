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

/*
 * In current implementation of CullList, this isn't used. It did odd things with a lot of sockets.
 */
bool CullList::IsValid(userrec* user)
{
	time_t esignon = 0;
	std::map<userrec*,time_t>::iterator es = exempt.find(user);
	if (es != exempt.end())
		esignon = es->second;

	for (user_hash::iterator u = ServerInstance->clientlist->begin(); u != ServerInstance->clientlist->end(); u++)
	{
		if (user == u->second)
			return (u->second->signon == esignon);
	}
	return false;
}

CullItem::CullItem(userrec* u, std::string &r)
{
	this->user = u;
	this->reason = r;
}

CullItem::CullItem(userrec* u, const char* r)
{
	this->user = u;
	this->reason = r;
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

CullList::CullList(InspIRCd* Instance) : ServerInstance(Instance)
{
	list.clear();
	exempt.clear();
}

void CullList::AddItem(userrec* user, std::string &reason)
{
	if (exempt.find(user) == exempt.end())
	{
		CullItem item(user,reason);
		list.push_back(item);
		exempt[user] = user->signon;
	}
}

void CullList::AddItem(userrec* user, const char* reason)
{
	if (exempt.find(user) == exempt.end())
	{
		CullItem item(user,reason);
		list.push_back(item);
		exempt[user] = user->signon;
	}
}

int CullList::Apply()
{
	int n = list.size();
	while (list.size())
	{
		std::vector<CullItem>::iterator a = list.begin();

		userrec::QuitUser(ServerInstance, a->GetUser(), a->GetReason().c_str());
		list.erase(list.begin());
	}
	return n;
}
