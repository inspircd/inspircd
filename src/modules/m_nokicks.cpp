/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Provides support for unreal-style channel mode +Q */

class NoKicks : public SimpleChannelModeHandler
{
 public:
	NoKicks(InspIRCd* Instance, Module* Creator) : SimpleChannelModeHandler(Instance, Creator, 'Q') { }
};

class ModuleNoKicks : public Module
{
	NoKicks nk;

 public:
	ModuleNoKicks(InspIRCd* Me)
		: Module(Me), nk(Me, this)
	{
		if (!ServerInstance->Modes->AddMode(&nk))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnAccessCheck, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('Q');
	}

	virtual int OnAccessCheck(User* source,User* dest,Channel* channel,int access_type)
	{
		if (access_type == AC_KICK)
		{
			if (channel->IsModeSet('Q') || channel->GetExtBanStatus(source, 'Q') < 0)
			{
				if ((ServerInstance->ULine(source->nick.c_str())) || (ServerInstance->ULine(source->server)) || (!*source->server))
				{
					// ulines can still kick with +Q in place
					return ACR_ALLOW;
				}
				else
				{
					// nobody else can (not even opers with override, and founders)
					source->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :Can't kick user %s from channel (+Q set)",source->nick.c_str(), channel->name.c_str(), dest->nick.c_str());
					return ACR_DENY;
				}
			}
		}
		return ACR_DEFAULT;
	}

	virtual ~ModuleNoKicks()
	{
		ServerInstance->Modes->DelMode(&nk);
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}
};


MODULE_INIT(ModuleNoKicks)
