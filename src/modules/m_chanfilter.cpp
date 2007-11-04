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
	ChanFilter(InspIRCd* Instance) : ListModeBase(Instance, 'g', "End of channel spamfilter list", "941", "940", false, "chanfilter") { }
	
	virtual bool ValidateParam(User* user, Channel* chan, std::string &word)
	{
		if ((word.length() > 35) || (word.empty()))
		{
			user->WriteServ("935 %s %s %s :word is too %s for censor list",user->nick, chan->name,word.c_str(), (word.empty() ? "short" : "long"));
			return false;
		}
		
		return true;
	}
	
	virtual bool TellListTooLong(User* user, Channel* chan, std::string &word)
	{
		user->WriteServ("939 %s %s %s :Channel spamfilter list is full",user->nick, chan->name, word.c_str());
		return true;
	}
	
	virtual void TellAlreadyOnList(User* user, Channel* chan, std::string &word)
	{
		user->WriteServ("937 %s %s :The word %s is already on the spamfilter list",user->nick, chan->name,word.c_str());
	}
	
	virtual void TellNotSet(User* user, Channel* chan, std::string &word)
	{
		user->WriteServ("938 %s %s :No such spamfilter word is set",user->nick, chan->name);
	}
};

class ModuleChanFilter : public Module
{
	
	ChanFilter* cf;
	
 public:
 
	ModuleChanFilter(InspIRCd* Me)
		: Module(Me)
	{
		cf = new ChanFilter(ServerInstance);
		if (!ServerInstance->AddMode(cf))
			throw ModuleException("Could not add new modes!");

		cf->DoImplements(this);
		Implementation eventlist[] = { I_OnCleanup, I_OnChannelDelete, I_OnRehash, I_OnUserPreMessage, I_OnUserPreNotice, I_OnSyncChannel };
		ServerInstance->Modules->Attach(eventlist, this, 6);
	}

	virtual void OnChannelDelete(Channel* chan)
	{
		cf->DoChannelDelete(chan);
	}

	virtual void OnRehash(User* user, const std::string &parameter)
	{
		cf->DoRehash();
	}

	virtual int ProcessMessages(User* user,Channel* chan,std::string &text)
	{
		if (!IS_LOCAL(user) || CHANOPS_EXEMPT(ServerInstance, 'g') && chan->GetStatus(user) == STATUS_OP)
			return 0;

		// Create a copy of the string in irc::string
		irc::string line = text.c_str();

		modelist* list;
		chan->GetExt(cf->GetInfoKey(), list);

		if (list)
		{
			for (modelist::iterator i = list->begin(); i != list->end(); i++)
			{
				if (line.find(i->mask.c_str()) != std::string::npos)
				{
					user->WriteServ("936 %s %s %s :Your message contained a censored word, and was blocked",user->nick, chan->name, i->mask.c_str());
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
		else return 0;
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		cf->DoCleanup(target_type, item);
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
		return Version(1, 1, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
	}
	
	virtual ~ModuleChanFilter()
	{
		ServerInstance->Modes->DelMode(cf);
		delete cf;
	}
};

MODULE_INIT(ModuleChanFilter)
