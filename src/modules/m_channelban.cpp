/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Implements extban +b j: - matching channel bans */

class ModuleBadChannelExtban : public Module
{
 private:
 public:
	void init() {
		Implementation eventlist[] = { I_OnCheckBan, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	Version GetVersion()
	{
		return Version("Extban 'j' - channel status/join ban", VF_OPTCOMMON|VF_VENDOR);
	}

	ModResult OnCheckBan(User *user, Channel *c, const std::string& mask)
	{
		if (mask[0] == 'j' && mask[1] == ':')
		{
			std::string rm = mask.substr(2);
			char status = 0;
			ModeHandler* mh = ServerInstance->Modes->FindPrefix(rm[0]);
			if (mh)
			{
				rm = mask.substr(3);
				status = mh->GetModeChar();
			}

			for (UCListIter i = user->chans.begin(); i != user->chans.end(); i++)
				if (InspIRCd::Match(i->chan->name, rm) && (!status || i->hasMode(status)))
					return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('j');
	}
};


MODULE_INIT(ModuleBadChannelExtban)

