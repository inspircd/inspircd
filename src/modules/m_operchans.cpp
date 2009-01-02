/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
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
		Implementation eventlist[] = { I_OnUserPreJoin };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}


	virtual int OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		if (!IS_OPER(user))
		{
			if (chan)
			{
				if (chan->IsModeSet('O'))
				{
					user->WriteNumeric(ERR_CANTJOINOPERSONLY, "%s %s :Only IRC operators may join the channel %s (+O is set)",user->nick.c_str(), chan->name.c_str(), chan->name.c_str());
					return 1;
				}
			}
		}
		else
		{
			if (chan && chan->IsExtBanned(user->oper, 'O'))
			{
				user->WriteNumeric(ERR_BANNEDFROMCHAN, "%s %s :Cannot join channel (You're banned)", user->nick.c_str(),  chan->name.c_str());
				return 1;
			}
		}

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
