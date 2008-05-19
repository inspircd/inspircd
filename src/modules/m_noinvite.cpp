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
		Implementation eventlist[] = { I_OnUserPreInvite };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}


	virtual int OnUserPreInvite(User* user,User* dest,Channel* channel, time_t timeout)
	{
		if (channel->IsModeSet('V'))
		{
			user->WriteNumeric(492, "%s %s :Can't invite %s to channel (+V set)",user->nick.c_str(), channel->name, dest->nick.c_str());
			return 1;
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
