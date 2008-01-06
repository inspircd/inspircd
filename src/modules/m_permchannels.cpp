/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Provides support for channel mode +P to provide permanent channels */


/** Handles the +P channel mode
 */
class PermChannel : public ModeHandler
{
 public:
	PermChannel(InspIRCd* Instance) : ModeHandler(Instance, 'P', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('P'))
			{
				channel->SetMode('P',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('P'))
			{
				channel->SetMode('P',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModulePermanentChannels : public Module
{
	PermChannel *p;
public:
	
	ModulePermanentChannels(InspIRCd* Me) : Module(Me)
	{
		p = new PermChannel(ServerInstance);
		if (!ServerInstance->AddMode(p))
		{
			delete p;
			throw ModuleException("Could not add new modes!");
		}
		Implementation eventlist[] = { I_OnChannelPreDelete };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}

	virtual ~ModulePermanentChannels()
	{
		ServerInstance->Modes->DelMode(p);
		delete p;
	}

	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_COMMON|VF_VENDOR,API_VERSION);
	}

	virtual int OnChannelPreDelete(Channel *c)
	{
		if (c->IsModeSet('P'))
			return 1;

		return 0;
	}
};

MODULE_INIT(ModulePermanentChannels)
