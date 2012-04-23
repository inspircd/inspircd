/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
 *   Copyright (C) 2005 Craig McLure <craig@chatspike.net>
 *   Copyright (C) 2005 Craig Edwards <craigedwards@brainbox.cc>
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


#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_DEPRECATE

#include "inspircd.h"
#include "u_listmode.h"

/* $ModDesc: Provides channel-specific censor lists (like mode +G but varies from channel to channel) */
/* $ModDep: ../../include/u_listmode.h */

/** Handles channel mode +g
 */
class ChanFilter : public ListModeBase
{
 public:
	ChanFilter(InspIRCd* Instance) : ListModeBase(Instance, 'g', "End of channel spamfilter list", 941, 940, false, "chanfilter") { }

	virtual bool ValidateParam(User* user, Channel* chan, std::string &word)
	{
		if ((word.length() > 35) || (word.empty()))
		{
			user->WriteNumeric(935, "%s %s %s :word is too %s for censor list",user->nick.c_str(), chan->name.c_str(), word.c_str(), (word.empty() ? "short" : "long"));
			return false;
		}

		return true;
	}

	virtual bool TellListTooLong(User* user, Channel* chan, std::string &word)
	{
		user->WriteNumeric(939, "%s %s %s :Channel spamfilter list is full", user->nick.c_str(), chan->name.c_str(), word.c_str());
		return true;
	}

	virtual void TellAlreadyOnList(User* user, Channel* chan, std::string &word)
	{
		user->WriteNumeric(937, "%s %s :The word %s is already on the spamfilter list",user->nick.c_str(), chan->name.c_str(), word.c_str());
	}

	virtual void TellNotSet(User* user, Channel* chan, std::string &word)
	{
		user->WriteNumeric(938, "%s %s :No such spamfilter word is set",user->nick.c_str(), chan->name.c_str());
	}
};

class ModuleChanFilter : public Module
{

	ChanFilter* cf;
	bool hidemask;

 public:

	ModuleChanFilter(InspIRCd* Me)
		: Module(Me)
	{
		cf = new ChanFilter(ServerInstance);
		if (!ServerInstance->Modes->AddMode(cf))
			throw ModuleException("Could not add new modes!");

		cf->DoImplements(this);
		Implementation eventlist[] = { I_OnCleanup, I_OnChannelDelete, I_OnRehash, I_OnUserPreMessage, I_OnUserPreNotice, I_OnSyncChannel };
		ServerInstance->Modules->Attach(eventlist, this, 6);

		OnRehash(NULL);
		ServerInstance->Modules->PublishInterface("ChannelBanList", this);
	}

	virtual void OnChannelDelete(Channel* chan)
	{
		cf->DoChannelDelete(chan);
	}

	virtual void OnRehash(User* user)
	{
		ConfigReader Conf(ServerInstance);
		hidemask = Conf.ReadFlag("chanfilter", "hidemask", 0);
		cf->DoRehash();
	}

	virtual int ProcessMessages(User* user,Channel* chan,std::string &text)
	{
		if (!IS_LOCAL(user) || (CHANOPS_EXEMPT(ServerInstance, 'g') && chan->GetStatus(user) == STATUS_OP))
			return 0;

		modelist* list;
		chan->GetExt(cf->GetInfoKey(), list);

		if (list)
		{
			for (modelist::iterator i = list->begin(); i != list->end(); i++)
			{
				if (InspIRCd::Match(text, i->mask))
				{
					if (hidemask)
						user->WriteNumeric(404, "%s %s :Cannot send to channel (your message contained a censored word)",user->nick.c_str(), chan->name.c_str());
					else
						user->WriteNumeric(404, "%s %s %s :Cannot send to channel (your message contained a censored word)",user->nick.c_str(), chan->name.c_str(), i->mask.c_str());
					return 1;
				}
			}
		}

		return 0;
	}

	virtual int OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type == TYPE_CHANNEL)
		{
			return ProcessMessages(user,(Channel*)dest,text);
		}
		return 0;
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		cf->DoCleanup(target_type, item);
	}

	virtual const char* OnRequest(Request* request)
	{
		return cf->DoOnRequest(request);
	}

	virtual int OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user,dest,target_type,text,status,exempt_list);
	}

	virtual void OnSyncChannel(Channel* chan, Module* proto, void* opaque)
	{
		cf->DoSyncChannel(chan, proto, opaque);
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}

	virtual ~ModuleChanFilter()
	{
		ServerInstance->Modes->DelMode(cf);
		delete cf;
		ServerInstance->Modules->UnpublishInterface("ChannelBanList", this);
	}
};

MODULE_INIT(ModuleChanFilter)
