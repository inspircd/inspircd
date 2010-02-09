/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_DEPRECATE

#include "inspircd.h"
#include "u_listmode.h"

/* $ModDesc: Provides channel-specific censor lists (like mode +G but varies from channel to channel) */

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

	ModuleChanFilter() : cf(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cf);

		Implementation eventlist[] = { I_OnRehash, I_OnUserPreMessage, I_OnUserPreNotice, I_OnSyncChannel };
		ServerInstance->Modules->Attach(eventlist, this, 4);

		OnRehash(NULL);
	}

	virtual void OnRehash(User* user)
	{
		ConfigReader Conf;
		hidemask = Conf.ReadFlag("chanfilter", "hidemask", 0);
		cf.DoRehash();
	}

	virtual ModResult ProcessMessages(User* user,Channel* chan,std::string &text)
	{
		ModResult res;
		FIRST_MOD_RESULT(OnChannelRestrictionApply, res, (user,chan,"filter"));

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

	virtual void OnCleanup(int target_type, void* item)
	{
		cf.DoCleanup(target_type, item);
	}

	virtual ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user,dest,target_type,text,status,exempt_list);
	}

	Version GetVersion()
	{
		return Version("Provides channel-specific censor lists (like mode +G but varies from channel to channel)", VF_VENDOR);
	}
};

MODULE_INIT(ModuleChanFilter)
