/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Provides support for unreal-style channel mode +C */

class NoCTCP : public SimpleChannelModeHandler
{
 public:
	NoCTCP(Module* Creator) : SimpleChannelModeHandler(Creator, "noctcp", 'C') { fixed_letter = false; }
};

class ModuleNoCTCP : public Module
{
	NoCTCP nc;
 public:
	ModuleNoCTCP() : nc(this) {}

	void init()
	{
		ServerInstance->Modules->AddService(nc);
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual ~ModuleNoCTCP()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for unreal-style channel mode +C", VF_VENDOR);
	}

	virtual ModResult OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreNotice(user,dest,target_type,text,status,exempt_list);
	}

	virtual ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if ((target_type == TYPE_CHANNEL) && (IS_LOCAL(user)))
		{
			Channel* c = (Channel*)dest;
			ModResult res = ServerInstance->CheckExemption(user,c,"noctcp");

			if (res == MOD_RES_ALLOW)
				return MOD_RES_PASSTHRU;

			if (!c->GetExtBanStatus(user, 'C').check(!c->IsModeSet(&nc)))
			{
				if ((text.length()) && (text[0] == '\1'))
				{
					if (strncmp(text.c_str(),"\1ACTION ",8))
					{
						user->WriteNumeric(ERR_NOCTCPALLOWED, "%s %s :Can't send CTCP to channel (+C set)",user->nick.c_str(), c->name.c_str());
						return MOD_RES_DENY;
					}
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('C');
	}
};

MODULE_INIT(ModuleNoCTCP)
