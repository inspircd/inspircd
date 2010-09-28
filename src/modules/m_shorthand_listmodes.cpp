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

/* $ModDesc: Allows shorthand listmodes (+v user1,user2,user3 instead of +vvv user1 user2 user3) */

#include "inspircd.h"

class ModuleShorthandListmodes : public Module
{
 public:
	ModuleShorthandListmodes() {}

	void init()
	{
		ServerInstance->Modules->Attach(I_OnPreMode, this);
	}

	virtual ~ModuleShorthandListmodes()
	{
	}

	ModResult OnPreMode(User* u, Extensible*, irc::modestacker& ms)
	{
		if(!IS_LOCAL(u)) return MOD_RES_PASSTHRU;
		std::vector<irc::modechange> newsequence;
		std::string token;
		for(std::vector<irc::modechange>::iterator iter = ms.sequence.begin(); iter != ms.sequence.end(); ++iter)
		{
			ModeHandler* mh = ServerInstance->Modes->FindMode(iter->mode);
			if(mh->IsListMode() && mh->GetTranslateType() == TR_NICK && iter->value.find_first_of(',') != std::string::npos)
			{
				irc::commasepstream sep(iter->value);
				while(sep.GetToken(token))
					newsequence.push_back(irc::modechange(iter->mode, token, iter->adding));
			}
			else
				newsequence.push_back(*iter);
		}
		ms.sequence.swap(newsequence);
		return MOD_RES_PASSTHRU;
	}

	virtual Version GetVersion()
	{
		return Version("Allows shorthand listmodes (+v user1,user2,user3 instead of +vvv user1 user2 user3)", VF_VENDOR);
	}
};

MODULE_INIT(ModuleShorthandListmodes)

