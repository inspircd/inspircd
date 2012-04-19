/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Provides support to block all-CAPS channel messages and notices */


/** Handles the +P channel mode
 */
class BlockCaps : public SimpleChannelModeHandler
{
 public:
	BlockCaps(InspIRCd* Instance) : SimpleChannelModeHandler(Instance, 'B') { }
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
		OnRehash(NULL);
		bc = new BlockCaps(ServerInstance);
		if (!ServerInstance->Modes->AddMode(bc))
		{
			delete bc;
			throw ModuleException("Could not add new modes!");
		}
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice, I_OnRehash, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 4);
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('B');
	}

	virtual void OnRehash(User* user)
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

			if (CHANOPS_EXEMPT(ServerInstance, 'B') && c->GetStatus(user) == STATUS_OP)
			{
				return 0;
			}

			if (c->IsModeSet('B') || c->GetExtBanStatus(user, 'B') < 0)
			{
				int caps = 0;
				const char* actstr = "\1ACTION ";
				int act = 0;

				for (std::string::iterator i = text.begin(); i != text.end(); i++)
				{
					/* Smart fix for suggestion from Jobe, ignore CTCP ACTION (part of /ME) */
					if (*actstr && *i == *actstr++ && act != -1)
					{
						act++;
						continue;
					}
					else
						act = -1;

					caps += capsmap[(unsigned char)*i];
				}
				if ( ((caps*100)/(int)text.length()) >= percent )
				{
					user->WriteNumeric(ERR_CANNOTSENDTOCHAN, "%s %s :Your message cannot contain more than %d%% capital letters if it's longer than %d characters", user->nick.c_str(), c->name.c_str(), percent, minlen);
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
		memset(capsmap, 0, sizeof(capsmap));
		for (std::string::iterator n = hmap.begin(); n != hmap.end(); n++)
			capsmap[(unsigned char)*n] = 1;
		if (percent < 1 || percent > 100)
		{
			ServerInstance->Logs->Log("CONFIG",DEFAULT, "<blockcaps:percent> out of range, setting to default of 100.");
			percent = 100;
		}
		if (minlen < 1 || minlen > MAXBUF-1)
		{
			ServerInstance->Logs->Log("CONFIG",DEFAULT, "<blockcaps:minlen> out of range, setting to default of 1.");
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
		return Version("$Id$", VF_COMMON|VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(ModuleBlockCAPS)
