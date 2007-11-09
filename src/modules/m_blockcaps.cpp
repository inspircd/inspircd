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

/* $ModDesc: Provides support for channel mode +P to block all-CAPS channel messages and notices */


/** Handles the +P channel mode
 */
class BlockCaps : public ModeHandler
{
 public:
	BlockCaps(InspIRCd* Instance) : ModeHandler(Instance, 'P', 0, 0, false, MODETYPE_CHANNEL, false) { }

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

class ModuleBlockCAPS : public Module
{
	BlockCaps* bc;
	int percent;
	unsigned int minlen;
	char capsmap[256];
public:
	
	ModuleBlockCAPS(InspIRCd* Me) : Module(Me)
	{
		OnRehash(NULL,"");
		bc = new BlockCaps(ServerInstance);
		if (!ServerInstance->AddMode(bc))
		{
			delete bc;
			throw ModuleException("Could not add new modes!");
		}
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}


	virtual void OnRehash(User* user, const std::string &param)
	{
		ReadConf();
	}

	virtual int OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type == TYPE_CHANNEL)
		{
			if ((!IS_LOCAL(user)) || (text.length() < minlen))
				return 0;

			Channel* c = (Channel*)dest;

			if (c->IsModeSet('P'))
			{
				int caps = 0;
				for (std::string::iterator i = text.begin(); i != text.end(); i++)
					caps += capsmap[(unsigned char)*i];
				if ( ((caps*100)/(int)text.length()) >= percent )
				{
					user->WriteServ( "404 %s %s :Your line cannot be more than %d%% capital letters if it is %d or more letters long", user->nick, c->name, percent, minlen);
					return 1;
				}
			}
		}
		return 0;
	}

	virtual int OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user,dest,target_type,text,status,exempt_list);
	}

	void ReadConf()
	{
		ConfigReader Conf(ServerInstance);
		percent = Conf.ReadInteger("blockcaps", "percent", "100", 0, true);
		minlen = Conf.ReadInteger("blockcaps", "minlen", "1", 0, true);
		std::string hmap = Conf.ReadValue("blockcaps", "capsmap", 0);
		if (hmap.empty())
			hmap = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		memset(&capsmap, 0, 255);
		for (std::string::iterator n = hmap.begin(); n != hmap.end(); n++)
			capsmap[(unsigned char)*n] = 1;
		if (percent < 1 || percent > 100)
		{
			ServerInstance->Log(DEFAULT, "<blockcaps:percent> out of range, setting to default of 100.");
			percent = 100;
		}
		if (minlen < 1 || minlen > MAXBUF-1)
		{
			ServerInstance->Log(DEFAULT, "<blockcaps:minlen> out of range, setting to default of 1.");
			minlen = 1;
		}
	}

	virtual ~ModuleBlockCAPS()
	{
		ServerInstance->Modes->DelMode(bc);
		delete bc;
	}

	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_COMMON|VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(ModuleBlockCAPS)
