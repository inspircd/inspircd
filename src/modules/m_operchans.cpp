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

/* $ModDesc: Provides support for oper-only chans via the +O channel mode */

class OperChans : public SimpleChannelModeHandler
{
 public:
	/* This is an oper-only mode */
	OperChans(Module* Creator) : SimpleChannelModeHandler(Creator, "operonly", 'O') { fixed_letter = false; }
};

class ModuleOperChans : public Module
{
	OperChans oc;
 public:
	ModuleOperChans() : oc(this) {}

	void init()
	{
		ServerInstance->Modules->AddService(oc);
		Implementation eventlist[] = { I_OnCheckBan, I_On005Numeric, I_OnCheckJoin };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void OnCheckJoin(ChannelPermissionData& join)
	{
		if (join.chan && join.result == MOD_RES_PASSTHRU && join.chan->IsModeSet(&oc) && !IS_OPER(join.user))
		{
			join.ErrorNumeric(ERR_CANTJOINOPERSONLY, "%s :Only IRC operators may join %s (+O is set)",
				join.chan->name.c_str(), join.chan->name.c_str());
			join.result = MOD_RES_DENY;
		}
	}

	ModResult OnCheckBan(User *user, Channel *c, const std::string& mask)
	{
		if (mask[0] == 'O' && mask[1] == ':')
		{
			if (IS_OPER(user) && InspIRCd::Match(user->oper->name, mask.substr(2)))
				return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('O');
	}

	~ModuleOperChans()
	{
	}

	Version GetVersion()
	{
		return Version("Provides support for oper-only chans via the +O channel mode and 'O' extban", VF_VENDOR);
	}
};

MODULE_INIT(ModuleOperChans)
