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

/* $ModDesc: Provides support to block all-CAPS channel messages and notices */


/** Handles the +B channel mode
 */
class BlockCaps : public SimpleChannelModeHandler
{
 public:
	BlockCaps(Module* Creator) : SimpleChannelModeHandler(Creator, "blockcaps", 'B') { fixed_letter = false; }
};

class ModuleBlockCAPS : public Module
{
	BlockCaps bc;
	int percent;
	unsigned int minlen;
	char capsmap[256];
public:

	ModuleBlockCAPS() : bc(this)
	{}

	void init()
	{
		ServerInstance->Modules->AddService(bc);
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('B');
	}

	virtual ModResult OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type == TYPE_CHANNEL)
		{
			if ((!IS_LOCAL(user)) || (text.length() < minlen))
				return MOD_RES_PASSTHRU;

			Channel* c = (Channel*)dest;
			ModResult res = ServerInstance->CheckExemption(user,c,"blockcaps");

			if (res == MOD_RES_ALLOW)
				return MOD_RES_PASSTHRU;

			if (!c->GetExtBanStatus(user, 'B').check(!c->IsModeSet(&bc)))
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
					user->WriteNumeric(ERR_CANNOTSENDTOCHAN, "%s %s :Your message must contain less than %d%% capital letters if it is %d characters or longer", user->nick.c_str(), c->name.c_str(), percent, minlen);
					return MOD_RES_DENY;
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}

	virtual ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user,dest,target_type,text,status,exempt_list);
	}

	void ReadConfig(ConfigReadStatus& status)
	{
		ConfigTag* tag = status.GetTag("blockcaps");
		percent = tag->getInt("percent", 100);
		minlen = tag->getInt("minlen", 1);
		std::string hmap = tag->getString("capsmap");
		if (hmap.empty())
			hmap = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		memset(capsmap, 0, sizeof(capsmap));
		for (std::string::iterator n = hmap.begin(); n != hmap.end(); n++)
			capsmap[(unsigned char)*n] = 1;
		if (percent < 1 || percent > 100)
		{
			status.ReportError(tag, "<blockcaps:percent> out of range, setting to default of 100.", false);
			percent = 100;
		}
		if (minlen < 1 || minlen > MAXBUF-1)
		{
			status.ReportError(tag, "<blockcaps:minlen> out of range, setting to default of 1.", false);
			minlen = 1;
		}
	}

	Version GetVersion()
	{
		return Version("Provides support to block all-CAPS channel messages and notices", VF_VENDOR);
	}
};

MODULE_INIT(ModuleBlockCAPS)
