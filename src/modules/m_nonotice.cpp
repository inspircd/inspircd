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

/* $ModDesc: Provides support for unreal-style channel mode +T */

class NoNotice : public SimpleChannelModeHandler
{
 public:
	NoNotice(Module* Creator) : SimpleChannelModeHandler(Creator, "nonotice", 'T') { }
};

class ModuleNoNotice : public Module
{
	NoNotice nt;
 public:

	ModuleNoNotice()
		: nt(this)
	{
		if (!ServerInstance->Modes->AddMode(&nt))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnUserPreNotice, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('T');
	}

	virtual ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		ModResult res;
		if ((target_type == TYPE_CHANNEL) && (IS_LOCAL(user)))
		{
			Channel* c = (Channel*)dest;
			if (!c->GetExtBanStatus(user, 'T').check(!c->IsModeSet('T')))
			{
				if (ServerInstance->ULine(user->server))
				{
					// ulines are exempt.
					return MOD_RES_PASSTHRU;
				}
				FIRST_MOD_RESULT(OnChannelRestrictionApply, res, (user,c,"nonotice"));
				if (res == MOD_RES_ALLOW)
					return MOD_RES_PASSTHRU;
				else
				{
					user->WriteNumeric(ERR_CANNOTSENDTOCHAN, "%s %s :Can't send NOTICE to channel (+T set)",user->nick.c_str(), c->name.c_str());
					return MOD_RES_DENY;
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}

	virtual ~ModuleNoNotice()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for unreal-style channel mode +T", VF_COMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleNoNotice)
