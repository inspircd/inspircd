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

#include "inspircd.h"

/* $ModDesc: Only opers may create new channels if this module is loaded */

class ModuleRestrictChans : public Module
{
	std::set<irc::string> allowchans;

	void ReadConfig()
	{
		allowchans.clear();
		ConfigTagList tags = ServerInstance->Config->GetTags("allowchannel");
		for(ConfigIter i = tags.first; i != tags.second; ++i)
		{
			ConfigTag* tag = i->second;
			std::string txt = tag->getString("name");
			allowchans.insert(txt.c_str());
		}
	}

 public:
	void init()
	{
		ReadConfig();
		Implementation eventlist[] = { I_OnCheckJoin };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void ReadConfig(ConfigReadStatus&)
	{
		ReadConfig();
	}


	void OnCheckJoin(ChannelPermissionData& join)
	{
		// channel does not yet exist (record is null, about to be created IF we were to allow it)
		if (!join.chan && join.result == MOD_RES_PASSTHRU && !IS_OPER(join.source))
		{
			// user is not an oper and its not in the allow list
			if (allowchans.find(join.channel) == allowchans.end())
			{
				join.ErrorNumeric(ERR_BANNEDFROMCHAN, "%s :Only IRC operators may create new channels",join.channel.c_str());
				join.result = MOD_RES_DENY;
			}
		}
	}

	virtual ~ModuleRestrictChans()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Only opers may create new channels if this module is loaded",VF_VENDOR);
	}
};

MODULE_INIT(ModuleRestrictChans)
