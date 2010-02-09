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

#include "inspircd.h"
#include "u_listmode.h"

/* $ModDesc: Provides support for the +e channel mode */

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
	BanException(Module* Creator) : ListModeBase(Creator, "banexception", 'e', "End of Channel Exception List", 348, 349, true) { }
};


class ModuleBanException : public Module
{
	BanException be;

public:
	ModuleBanException() : be(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(be);

		Implementation list[] = { I_OnRehash, I_On005Numeric, I_OnExtBanCheck, I_OnCheckChannelBan };
		ServerInstance->Modules->Attach(list, this, 4);
	}

	void On005Numeric(std::string &output)
	{
		output.append(" EXCEPTS=e");
	}

	ModResult OnExtBanCheck(User *user, Channel *chan, char type)
	{
		if (chan != NULL)
		{
			modelist *list = be.extItem.get(chan);

			if (!list)
				return MOD_RES_PASSTHRU;

			for (modelist::iterator it = list->begin(); it != list->end(); it++)
			{
				if (it->mask[0] != type || it->mask[1] != ':')
					continue;

				if (chan->CheckBan(user, it->mask.substr(2)))
				{
					// They match an entry on the list, so let them pass this.
					return MOD_RES_ALLOW;
				}
			}
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnCheckChannelBan(User* user, Channel* chan)
	{
		if (chan)
		{
			modelist *list = be.extItem.get(chan);

			if (!list)
			{
				// No list, proceed normally
				return MOD_RES_PASSTHRU;
			}

			for (modelist::iterator it = list->begin(); it != list->end(); it++)
			{
				if (chan->CheckBan(user, it->mask))
				{
					// They match an entry on the list, so let them in.
					return MOD_RES_ALLOW;
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}

	void OnRehash(User* user)
	{
		be.DoRehash();
	}

	Version GetVersion()
	{
		return Version("Provides support for the +e channel mode", VF_VENDOR);
	}
};

MODULE_INIT(ModuleBanException)
