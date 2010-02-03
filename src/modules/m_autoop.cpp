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

/* $ModDesc: Provides support for the +w channel mode, autoop list */

/** Handles +w channel mode
 */
class AutoOpList : public ListModeBase
{
 public:
	AutoOpList(Module* Creator) : ListModeBase(Creator, "autoop", 'w', "End of Channel Access List", 910, 911, true)
	{
		levelrequired = OP_VALUE;
	}
	// TODO need own numerics
	// TODO add some serious access control for setting this mode (you can currently gain +qa with it)
};


class ModuleAutoOp : public Module
{
	AutoOpList mh;

public:
	ModuleAutoOp() : mh(this)
	{
		ServerInstance->Modules->AddService(mh);
		mh.DoImplements(this);

		Implementation list[] = { I_OnUserPreJoin, };
		ServerInstance->Modules->Attach(list, this, 1);
	}

	ModResult OnUserPreJoin(User *user, Channel *chan, const char *cname, std::string &privs, const std::string &keygiven)
	{
		if (!chan)
			return MOD_RES_PASSTHRU;

		modelist* list = mh.extItem.get(chan);
		if (list)
		{
			for (modelist::iterator it = list->begin(); it != list->end(); it++)
			{
				std::string::size_type colon = it->mask.find(':');
				if (colon == std::string::npos)
					continue;
				if (chan->CheckBan(user, it->mask.substr(colon+1)))
				{
					privs = it->mask.substr(0, colon);
					break;
				}
			}
		}

		return MOD_RES_PASSTHRU;
	}

	void OnCleanup(int target_type, void* item)
	{
		mh.DoCleanup(target_type, item);
	}

	void OnSyncChannel(Channel* chan, Module* proto, void* opaque)
	{
		mh.DoSyncChannel(chan, proto, opaque);
	}

	void OnRehash(User* user)
	{
		mh.DoRehash();
	}

	Version GetVersion()
	{
		return Version("Provides support for the +w channel mode", VF_VENDOR);
	}
};

MODULE_INIT(ModuleAutoOp)
