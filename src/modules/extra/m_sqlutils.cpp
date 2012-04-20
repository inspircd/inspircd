/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
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
#include <sstream>
#include <list>
#include "m_sqlutils.h"

/* $ModDesc: Provides some utilities to SQL client modules, such as mapping queries to users and channels */
/* $ModDep: m_sqlutils.h */

typedef std::map<unsigned long, User*> IdUserMap;
typedef std::map<unsigned long, Channel*> IdChanMap;
typedef std::list<unsigned long> AssocIdList;

class ModuleSQLutils : public Module
{
private:
	IdUserMap iduser;
	IdChanMap idchan;

public:
	ModuleSQLutils(InspIRCd* Me)
	: Module(Me)
	{
		ServerInstance->Modules->PublishInterface("SQLutils", this);
		Implementation eventlist[] = { I_OnChannelDelete, I_OnUnloadModule, I_OnRequest, I_OnUserDisconnect };
		ServerInstance->Modules->Attach(eventlist, this, 4);
	}

	virtual ~ModuleSQLutils()
	{
		ServerInstance->Modules->UnpublishInterface("SQLutils", this);
	}


	virtual const char* OnRequest(Request* request)
	{
		if(strcmp(SQLUTILAU, request->GetId()) == 0)
		{
			AssociateUser* req = (AssociateUser*)request;

			iduser.insert(std::make_pair(req->id, req->user));

			AttachList(req->user, req->id);
		}
		else if(strcmp(SQLUTILAC, request->GetId()) == 0)
		{
			AssociateChan* req = (AssociateChan*)request;

			idchan.insert(std::make_pair(req->id, req->chan));

			AttachList(req->chan, req->id);
		}
		else if(strcmp(SQLUTILUA, request->GetId()) == 0)
		{
			UnAssociate* req = (UnAssociate*)request;

			/* Unassociate a given query ID with all users and channels
			 * it is associated with.
			 */

			DoUnAssociate(iduser, req->id);
			DoUnAssociate(idchan, req->id);
		}
		else if(strcmp(SQLUTILGU, request->GetId()) == 0)
		{
			GetAssocUser* req = (GetAssocUser*)request;

			IdUserMap::iterator iter = iduser.find(req->id);

			if(iter != iduser.end())
			{
				req->user = iter->second;
			}
		}
		else if(strcmp(SQLUTILGC, request->GetId()) == 0)
		{
			GetAssocChan* req = (GetAssocChan*)request;

			IdChanMap::iterator iter = idchan.find(req->id);

			if(iter != idchan.end())
			{
				req->chan = iter->second;
			}
		}

		return SQLUTILSUCCESS;
	}

	virtual void OnUserDisconnect(User* user)
	{
		/* A user is disconnecting, first we need to check if they have a list of queries associated with them.
		 * Then, if they do, we need to erase each of them from our IdUserMap (iduser) so when the module that
		 * associated them asks to look them up then it gets a NULL result and knows to discard the query.
		 */
		AssocIdList* il;

		if(user->GetExt("sqlutils_queryids", il))
		{
			for(AssocIdList::iterator listiter = il->begin(); listiter != il->end(); listiter++)
			{
				IdUserMap::iterator iter;

				iter = iduser.find(*listiter);

				if(iter != iduser.end())
				{
					if(iter->second != user)
					{
						ServerInstance->Logs->Log("m_sqlutils",DEBUG, "BUG: ID associated with user %s doesn't have the same User* associated with it in the map (erasing anyway)", user->nick.c_str());
					}

					iduser.erase(iter);
				}
				else
				{
					ServerInstance->Logs->Log("m_sqlutils",DEBUG, "BUG: user %s was extended with sqlutils_queryids but there was nothing matching in the map", user->nick.c_str());
				}
			}

			user->Shrink("sqlutils_queryids");
			delete il;
		}
	}

	void AttachList(Extensible* obj, unsigned long id)
	{
		AssocIdList* il;

		if(!obj->GetExt("sqlutils_queryids", il))
		{
			/* Doesn't already exist, create a new list and attach it. */
			il = new AssocIdList;
			obj->Extend("sqlutils_queryids", il);
		}

		/* Now either way we have a valid list in il, attached. */
		il->push_back(id);
	}

	void RemoveFromList(Extensible* obj, unsigned long id)
	{
		AssocIdList* il;

		if(obj->GetExt("sqlutils_queryids", il))
		{
			/* Only do anything if the list exists... (which it ought to) */
			il->remove(id);

			if(il->empty())
			{
				/* If we just emptied it.. */
				delete il;
				obj->Shrink("sqlutils_queryids");
			}
		}
	}

	template <class T> void DoUnAssociate(T &map, unsigned long id)
	{
		/* For each occurence of 'id' (well, only one..it's not a multimap) in 'map'
		 * remove it from the map, take an Extensible* value from the map and remove
		 * 'id' from the list of query IDs attached to it.
		 */
		typename T::iterator iter = map.find(id);

		if(iter != map.end())
		{
			/* Found a value indexed by 'id', call RemoveFromList()
			 * on it with 'id' to remove 'id' from the list attached
			 * to the value.
			 */
			RemoveFromList(iter->second, id);
		}
	}

	virtual void OnChannelDelete(Channel* chan)
	{
		/* A channel is being destroyed, first we need to check if it has a list of queries associated with it.
		 * Then, if it does, we need to erase each of them from our IdChanMap (idchan) so when the module that
		 * associated them asks to look them up then it gets a NULL result and knows to discard the query.
		 */
		AssocIdList* il;

		if(chan->GetExt("sqlutils_queryids", il))
		{
			for(AssocIdList::iterator listiter = il->begin(); listiter != il->end(); listiter++)
			{
				IdChanMap::iterator iter;

				iter = idchan.find(*listiter);

				if(iter != idchan.end())
				{
					if(iter->second != chan)
					{
						ServerInstance->Logs->Log("m_sqlutils",DEBUG, "BUG: ID associated with channel %s doesn't have the same Channel* associated with it in the map (erasing anyway)", chan->name.c_str());
					}
					idchan.erase(iter);
				}
				else
				{
					ServerInstance->Logs->Log("m_sqlutils",DEBUG, "BUG: channel %s was extended with sqlutils_queryids but there was nothing matching in the map", chan->name.c_str());
				}
			}

			chan->Shrink("sqlutils_queryids");
			delete il;
		}
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR | VF_SERVICEPROVIDER, API_VERSION);
	}

};

MODULE_INIT(ModuleSQLutils)
