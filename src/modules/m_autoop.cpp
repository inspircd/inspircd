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
		tidy = false;
	}

	ModeHandler* FindMode(const std::string& mid)
	{
		if (mid.length() == 1)
			return ServerInstance->Modes->FindMode(mid[0], MODETYPE_CHANNEL);
		for(char c='A'; c < 'z'; c++)
		{
			ModeHandler* mh = ServerInstance->Modes->FindMode(c, MODETYPE_CHANNEL);
			if (mh && mh->name == mid)
				return mh;
		}
		return NULL;
	}

	ModResult AccessCheck(User* source, Channel* channel, std::string &parameter, bool adding)
	{
		std::string::size_type pos = parameter.find(':');
		if (pos == 0 || pos == std::string::npos)
			return adding ? MOD_RES_DENY : MOD_RES_PASSTHRU;
		unsigned int mylevel = channel->GetPrefixValue(source);
		std::string mid = parameter.substr(0, pos);
		ModeHandler* mh = FindMode(mid);

		if (adding && (!mh || !mh->GetPrefixRank()))
		{
			source->WriteNumeric(415, "%s %s :Cannot find prefix mode '%s' for autoop",
				source->nick.c_str(), mid.c_str(), mid.c_str());
			return MOD_RES_DENY;
		}
		else if (!mh)
			return MOD_RES_PASSTHRU;

		std::string dummy;
		if (mh->AccessCheck(source, channel, dummy, true) == MOD_RES_DENY)
			return MOD_RES_DENY;
		if (mh->GetLevelRequired() > mylevel)
		{
			source->WriteNumeric(482, "%s %s :You must be able to set mode '%s' to include it in an autoop",
				source->nick.c_str(), channel->name.c_str(), mid.c_str());
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}
};

class ModuleAutoOp : public Module
{
	AutoOpList mh;

public:
	ModuleAutoOp() : mh(this)
	{
		ServerInstance->Modules->AddService(mh);
		mh.DoImplements(this);

		Implementation list[] = { I_OnPostJoin, };
		ServerInstance->Modules->Attach(list, this, 1);
	}

	void OnPostJoin(Membership *memb)
	{
		if (!IS_LOCAL(memb->user))
			return;

		modelist* list = mh.extItem.get(memb->chan);
		if (list)
		{
			std::string modeline("+");
			std::vector<std::string> modechange;
			modechange.push_back(memb->chan->name);
			for (modelist::iterator it = list->begin(); it != list->end(); it++)
			{
				std::string::size_type colon = it->mask.find(':');
				if (colon == std::string::npos)
					continue;
				if (memb->chan->CheckBan(memb->user, it->mask.substr(colon+1)))
				{
					ModeHandler* given = mh.FindMode(it->mask.substr(0, colon));
					if (given && given->GetPrefixRank())
						modeline.push_back(given->GetModeChar());
				}
			}
			modechange.push_back(modeline);
			for(std::string::size_type i = modeline.length(); i > 1; --i) // we use "i > 1" instead of "i" so we skip the +
				modechange.push_back(memb->user->nick);
			if(modechange.size() >= 3)
				ServerInstance->SendMode(modechange,ServerInstance->FakeClient);
		}
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
