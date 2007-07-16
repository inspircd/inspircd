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
#include "channels.h"
#include "modules.h"
#include "mode.h"
#include "u_listmode.h"
#include "wildcard.h"

/* $ModDesc: Provides support for the +e channel mode */
/* $ModDep: ../../include/u_listmode.h */

/* Written by Om<om@inspircd.org>, April 2005. */
/* Rewritten to use the listmode utility by Om, December 2005 */
/* Adapted from m_exception, which was originally based on m_chanprotect and m_silence */

// The +e channel mode takes a nick!ident@host, glob patterns allowed,
// and if a user matches an entry on the +e list then they can join the channel, overriding any (+b) bans set on them
// Now supports CIDR and IP addresses -- Brain


/** Handles +e channel mode
 */
class BanException : public ListModeBase
{
 public:
	BanException(InspIRCd* Instance) : ListModeBase(Instance, 'e', "End of Channel Exception List", "348", "349", true) { }
};


class ModuleBanException : public Module
{
	BanException* be;
	

public:
	ModuleBanException(InspIRCd* Me)
	: Module(Me)
	{
		be = new BanException(ServerInstance);
		if (!ServerInstance->AddMode(be, 'e'))
			throw ModuleException("Could not add new modes!");
		ServerInstance->PublishInterface("ChannelBanList", this);
	}
	
	virtual void Implements(char* List)
	{
		be->DoImplements(List);
		List[I_OnRehash] = List[I_OnRequest] = List[I_On005Numeric] = List[I_OnCheckBan] = 1;
	}
	
	virtual void On005Numeric(std::string &output)
	{
		output.append(" EXCEPTS=e");
	}

	virtual int OnCheckBan(userrec* user, chanrec* chan)
	{
		if (chan != NULL)
		{
			modelist* list;
			chan->GetExt(be->GetInfoKey(), list);
			
			if (list)
			{
				char mask[MAXBUF];
				snprintf(mask, MAXBUF, "%s!%s@%s", user->nick, user->ident, user->GetIPString());
				for (modelist::iterator it = list->begin(); it != list->end(); it++)
				{
					if (match(user->GetFullRealHost(), it->mask.c_str()) || match(user->GetFullHost(), it->mask.c_str()) || (match(mask, it->mask.c_str(), true)))
					{
						// They match an entry on the list, so let them in.
						return 1;
					}
				}
				return 0;
			}
			// or if there wasn't a list, there can't be anyone on it, so we don't need to do anything.
		}
		return 0;	
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		be->DoCleanup(target_type, item);
	}

	virtual void OnSyncChannel(chanrec* chan, Module* proto, void* opaque)
	{
		be->DoSyncChannel(chan, proto, opaque);
	}

	virtual void OnChannelDelete(chanrec* chan)
	{
		be->DoChannelDelete(chan);
	}

	virtual void OnRehash(userrec* user, const std::string &param)
	{
		be->DoRehash();
	}

	virtual char* OnRequest(Request* request)
	{
		ListModeRequest* LM = (ListModeRequest*)request;
		if (strcmp("LM_CHECKLIST", request->GetId()) == 0)
		{
			modelist* list;
			LM->chan->GetExt(be->GetInfoKey(), list);
			if (list)
			{
				char mask[MAXBUF];
				snprintf(mask, MAXBUF, "%s!%s@%s", LM->user->nick, LM->user->ident, LM->user->GetIPString());
				for (modelist::iterator it = list->begin(); it != list->end(); it++)
				{
					if (match(LM->user->GetFullRealHost(), it->mask.c_str()) || match(LM->user->GetFullHost(), it->mask.c_str()) || (match(mask, it->mask.c_str(), true)))
					{
						// They match an entry
						return (char*)it->mask.c_str();
					}
				}
				return NULL;
			}
		}
		return NULL;
	}

	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 3, VF_COMMON | VF_VENDOR, API_VERSION);
	}
	
	virtual ~ModuleBanException()
	{
		ServerInstance->Modes->DelMode(be);
		DELETE(be);
		ServerInstance->UnpublishInterface("ChannelBanList", this);
	}
};

MODULE_INIT(ModuleBanException)
