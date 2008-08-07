/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Provides support for unreal-style channel mode +V */

class NoInvite : public SimpleChannelModeHandler
{
 public:
	NoInvite(InspIRCd* Instance) : SimpleChannelModeHandler(Instance, 'V') { }
};

class ModuleNoInvite : public Module
{
	NoInvite *ni;
 public:

	ModuleNoInvite(InspIRCd* Me) : Module(Me)
	{
		ni = new NoInvite(ServerInstance);
		if (!ServerInstance->Modes->AddMode(ni))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnUserPreInvite, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('V');
	}

	virtual int OnUserPreInvite(User* user,User* dest,Channel* channel, time_t timeout)
	{
		if (IS_LOCAL(user))
		{
			if (CHANOPS_EXEMPT(ServerInstance, 'V') && channel->GetStatus(user) == STATUS_OP)
			{
				return 0;
			}

			if (channel->IsModeSet('V') || channel->IsExtBanned(user, 'V'))
			{
				user->WriteNumeric(ERR_NOCTCPALLOWED, "%s %s :Can't invite %s to channel (+V set)",user->nick.c_str(), channel->name.c_str(), dest->nick.c_str());
				return 1;
			}
		}

		return 0;
	}

	virtual ~ModuleNoInvite()
	{
		ServerInstance->Modes->DelMode(ni);
		delete ni;
	}

	virtual Version GetVersion()
	{
		return Version(1,2,0,0,VF_COMMON|VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(ModuleNoInvite)
