/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
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

/*
 * Written by Om <om@inspircd.org>, April 2005.
 * Based on m_exception, which was originally based on m_chanprotect and m_silence
 *
 * The +I channel mode takes a nick!ident@host, glob patterns allowed,
 * and if a user matches an entry on the +I list then they can join the channel,
 * ignoring if +i is set on the channel
 * Now supports CIDR and IP addresses -- Brain
 */

/** Handles channel mode +I
 */
class InviteException : public ListModeBase
{
 public:
	InviteException(Module* Creator) : ListModeBase(Creator, "invex", 'I', "End of Channel Invite Exception List", 346, 347, true) { fixed_letter = false; }
};

class ModuleInviteException : public Module
{
	InviteException ie;
public:
	ModuleInviteException() : ie(this)
	{
	}

	void init()
	{
		ie.init();
		ServerInstance->Modules->AddService(ie);

		Implementation eventlist[] = { I_On005Numeric, I_OnCheckJoin, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	void On005Numeric(std::string &output)
	{
		if (ie.GetModeChar())
			output.append(" INVEX=").push_back(ie.GetModeChar());
	}

	void OnCheckJoin(ChannelPermissionData& join)
	{
		if(!join.chan)
			return;
		modelist* list = ie.extItem.get(join.chan);
		if (!list)
			return;
		for (modelist::iterator it = list->begin(); it != list->end(); it++)
		{
			if (join.chan->CheckBan(join.user, (**it).mask))
				join.needs_invite = false;
		}
	}

	void OnRehash(User* user)
	{
		ie.DoRehash();
	}

	Version GetVersion()
	{
		return Version("Provides support for the +I channel mode", VF_VENDOR);
	}
};

MODULE_INIT(ModuleInviteException)
