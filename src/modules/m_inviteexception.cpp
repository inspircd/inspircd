/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "u_listmode.h"

/* $ModDesc: Provides support for the +I channel mode */
/* $ModDep: ../../include/u_listmode.h */

/*
 * Written by Om <om@inspircd.org>, April 2005.
 * Based on m_exception, which was originally based on m_chanprotect and m_silence
 *
 * The +I channel mode takes a nick!ident@host, glob patterns allowed,
 * and if a user matches an entry on the +I list then they can join the channel,
 * ignoring if +i is set on the channel
 * Now supports CIDR and IP addresses -- Brain
 */

class InspIRCd* ServerInstance;

/** Handles channel mode +I
 */
class InviteException : public ListModeBase
{
 public:
	InviteException(InspIRCd* Instance, Module* Creator) : ListModeBase(Instance, Creator, 'I', "End of Channel Invite Exception List", 346, 347, true) { }
};

class ModuleInviteException : public Module
{
	InviteException ie;
public:
	ModuleInviteException(InspIRCd* Me) : Module(Me), ie(Me, this)
	{
		if (!ServerInstance->Modes->AddMode(&ie))
			throw ModuleException("Could not add new modes!");
		ServerInstance->Modules->PublishInterface("ChannelBanList", this);

		ie.DoImplements(this);
		Implementation eventlist[] = { I_OnRequest, I_On005Numeric, I_OnCheckInvite };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	void On005Numeric(std::string &output)
	{
		output.append(" INVEX=I");
	}

	ModResult OnCheckInvite(User* user, Channel* chan)
	{
		if(chan != NULL)
		{
			modelist* list = ie.extItem.get(chan);
			if (list)
			{
				std::string mask = std::string(user->nick) + "!" + user->ident + "@" + user->GetIPString();
				for (modelist::iterator it = list->begin(); it != list->end(); it++)
				{
					if (chan->CheckBan(user, it->mask))
					{
						return MOD_RES_ALLOW;
					}
				}
			}
			// or if there wasn't a list, there can't be anyone on it, so we don't need to do anything.
		}

		return MOD_RES_PASSTHRU;
	}

	const char* OnRequest(Request* request)
	{
		return ie.DoOnRequest(request);
	}

	void OnCleanup(int target_type, void* item)
	{
		ie.DoCleanup(target_type, item);
	}

	void OnSyncChannel(Channel* chan, Module* proto, void* opaque)
	{
		ie.DoSyncChannel(chan, proto, opaque);
	}

	void OnRehash(User* user)
	{
		ie.DoRehash();
	}

	Version GetVersion()
	{
		return Version("Provides support for the +I channel mode", VF_VENDOR | VF_COMMON, API_VERSION);
	}

	~ModuleInviteException()
	{
		ServerInstance->Modes->DelMode(&ie);
		ServerInstance->Modules->UnpublishInterface("ChannelBanList", this);
	}
};

MODULE_INIT(ModuleInviteException)
