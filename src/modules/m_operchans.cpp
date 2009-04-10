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

/* $ModDesc: Provides support for oper-only chans via the +O channel mode */

class OperChans : public ModeHandler
{
 public:
	/* This is an oper-only mode */
	OperChans(InspIRCd* Instance) : ModeHandler(Instance, 'O', 0, 0, false, MODETYPE_CHANNEL, true) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool)
	{
		if (adding)
		{
			if (!channel->IsModeSet('O'))
			{
				channel->SetMode('O',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('O'))
			{
				channel->SetMode('O',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleOperChans : public Module
{

	OperChans* oc;
 public:
	ModuleOperChans(InspIRCd* Me)
		: Module(Me)
	{

		oc = new OperChans(ServerInstance);
		if (!ServerInstance->Modes->AddMode(oc))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnCheckBan, I_OnUserPreJoin };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual int OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		if (chan && chan->IsModeSet('O') && !IS_OPER(user))
		{
			user->WriteNumeric(ERR_CANTJOINOPERSONLY, "%s %s :Only IRC operators may join %s (+O is set)",
				user->nick.c_str(), chan->name.c_str(), chan->name.c_str());
			return 1;
		}
		return 0;
	}

	virtual int OnCheckBan(User* user, Channel* chan)
	{
		if (IS_OPER(user))
			return chan->GetExtBanStatus(user->oper, 'O');

		return 0;
	}

	virtual ~ModuleOperChans()
	{
		ServerInstance->Modes->DelMode(oc);
		delete oc;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR | VF_COMMON, API_VERSION);
	}
};

MODULE_INIT(ModuleOperChans)
