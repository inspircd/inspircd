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

#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_DEPRECATE

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"

typedef std::map<irc::string,irc::string> censor_t;

/* $ModDesc: Provides user and channel +G mode */

/** Handles usermode +G
 */
class CensorUser : public ModeHandler
{
 public:
	CensorUser(InspIRCd* Instance) : ModeHandler(Instance, 'G', 0, 0, false, MODETYPE_USER, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!dest->IsModeSet('G'))
			{
				dest->SetMode('G',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('G'))
			{
				dest->SetMode('G',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

/** Handles channel mode +G
 */
class CensorChannel : public ModeHandler
{
 public:
	CensorChannel(InspIRCd* Instance) : ModeHandler(Instance, 'G', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('G'))
			{
				channel->SetMode('G',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('G'))
			{
				channel->SetMode('G',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_ALLOW;
	}
};

class ModuleCensor : public Module
{

	
	censor_t censors;
	CensorUser *cu;
	CensorChannel *cc;
 
 public:
	ModuleCensor(InspIRCd* Me)
		: Module(Me)
	{
		/* Read the configuration file on startup.
		 */
		OnRehash(NULL,"");
		cu = new CensorUser(ServerInstance);
		cc = new CensorChannel(ServerInstance);
		if (!ServerInstance->AddMode(cu, 'G') || !ServerInstance->AddMode(cc, 'G'))
			throw ModuleException("Could not add new modes!");
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = List[I_OnUserPreMessage] = List[I_OnUserPreNotice] = 1;
	}

	virtual ~ModuleCensor()
	{
		ServerInstance->Modes->DelMode(cu);
		ServerInstance->Modes->DelMode(cc);
		DELETE(cu);
		DELETE(cc);
	}

	virtual void ReplaceLine(irc::string &text, irc::string pattern, irc::string replace)
	{
		if ((!pattern.empty()) && (!text.empty()))
		{
			std::string::size_type pos;
			while ((pos = text.find(pattern)) != irc::string::npos)
			{
				text.erase(pos,pattern.length());
				text.insert(pos,replace);
			}
		}
	}

	// format of a config entry is <badword text="shit" replace="poo">
	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (!IS_LOCAL(user))
			return 0;

		bool active = false;

		if (target_type == TYPE_USER)
			active = ((userrec*)dest)->IsModeSet('G');
		else if (target_type == TYPE_CHANNEL)
			active = ((chanrec*)dest)->IsModeSet('G');

		if (!active)
			return 0;

		irc::string text2 = text.c_str();
		for (censor_t::iterator index = censors.begin(); index != censors.end(); index++)
		{ 
			if (text2.find(index->first) != irc::string::npos)
			{
				if (index->second.empty())
				{
					user->WriteServ("936 %s %s %s :Your message contained a censored word, and was blocked", user->nick, ((chanrec*)dest)->name, index->first.c_str());
					return 1;
				}
				
				this->ReplaceLine(text2,index->first,index->second);
			}
		}
		text = text2.c_str();
		return 0;
	}
	
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user,dest,target_type,text,status,exempt_list);
	}
	
	virtual void OnRehash(userrec* user, const std::string &parameter)
	{
		/*
		 * reload our config file on rehash - we must destroy and re-allocate the classes
		 * to call the constructor again and re-read our data.
		 */
		ConfigReader* MyConf = new ConfigReader(ServerInstance);
		censors.clear();
		for (int index = 0; index < MyConf->Enumerate("badword"); index++)
		{
			irc::string pattern = (MyConf->ReadValue("badword","text",index)).c_str();
			irc::string replace = (MyConf->ReadValue("badword","replace",index)).c_str();
			censors[pattern] = replace;
		}
		DELETE(MyConf);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_COMMON|VF_VENDOR,API_VERSION);
	}
	
};

MODULE_INIT(ModuleCensor)
