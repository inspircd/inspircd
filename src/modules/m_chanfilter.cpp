/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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


/* $ModDesc: Provides channel-specific censor lists (like mode +G but varies from channel to channel) */

#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_DEPRECATE

#include "inspircd.h"
#include "u_listmode.h"

/** Handles channel mode +g
 */
class ChanFilter : public ListModeBase
{
 public:
	ChanFilter(Module* Creator) : ListModeBase(Creator, "filter", 'g', "End of channel spamfilter list", 941, 940, false, "chanfilter") { }

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
	ChanFilter cf;
	bool hidemask;

 public:

	ModuleChanFilter()
		: cf(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cf);

		cf.DoImplements(this);
		Implementation eventlist[] = { I_OnRehash, I_OnUserPreMessage, I_OnUserPreNotice, I_OnSyncChannel };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));

		OnRehash(NULL);
	}

	virtual void OnRehash(User* user)
	{
		hidemask = ServerInstance->Config->ConfValue("chanfilter")->getBool("hidemask");
		cf.DoRehash();
	}

	virtual ModResult ProcessMessages(User* user,Channel* chan,std::string &text)
	{
		ModResult res = ServerInstance->OnCheckExemption(user,chan,"filter");

		if (!IS_LOCAL(user) || res == MOD_RES_ALLOW)
			return MOD_RES_PASSTHRU;

		modelist* list = cf.extItem.get(chan);

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
					return MOD_RES_DENY;
				}
			}
		}

		return MOD_RES_PASSTHRU;
	}

	virtual ModResult OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type == TYPE_CHANNEL)
		{
			return ProcessMessages(user,(Channel*)dest,text);
		}
		return MOD_RES_PASSTHRU;
	}

	virtual ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user,dest,target_type,text,status,exempt_list);
	}

	virtual void OnSyncChannel(Channel* chan, Module* proto, void* opaque)
	{
		cf.DoSyncChannel(chan, proto, opaque);
	}

	virtual Version GetVersion()
	{
		return Version("Provides channel-specific censor lists (like mode +G but varies from channel to channel)", VF_VENDOR);
	}

	virtual ~ModuleChanFilter()
	{
	}
};

MODULE_INIT(ModuleChanFilter)
