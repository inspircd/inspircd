/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
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
	

	std::map<irc::string,int> allowchans;

	void ReadConfig()
	{
		ConfigReader* MyConf = new ConfigReader(ServerInstance);
		allowchans.clear();
		for (int i = 0; i < MyConf->Enumerate("allowchannel"); i++)
		{
			std::string txt;
			txt = MyConf->ReadValue("allowchannel", "name", i);
			irc::string channel = txt.c_str();
			allowchans[channel] = 1;
		}
		delete MyConf;
	}

 public:
	ModuleRestrictChans(InspIRCd* Me)
		: Module(Me)
	{
		
		ReadConfig();
		Implementation eventlist[] = { I_OnUserPreJoin, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual void OnRehash(User* user, const std::string &parameter)
	{
		ReadConfig();
	}

	
	virtual int OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs)
	{
		irc::string x = cname;
		// user is not an oper and its not in the allow list
		if ((!IS_OPER(user)) && (allowchans.find(x) == allowchans.end()))
		{
			// channel does not yet exist (record is null, about to be created IF we were to allow it)
			if (!chan)
			{
				user->WriteServ("530 %s %s :Only IRC operators may create new channels",user->nick,cname,cname);
				return 1;
			}
		}
		return 0;
	}
	
	virtual ~ModuleRestrictChans()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(ModuleRestrictChans)
