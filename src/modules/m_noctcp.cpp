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

/* $ModDesc: Provides support for unreal-style channel mode +C */

class NoCTCP : public ModeHandler
{
 public:
	NoCTCP(Module* Creator) : ModeHandler(Creator, 'C', PARAM_NONE, MODETYPE_CHANNEL) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('C'))
			{
				channel->SetMode('C',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('C'))
			{
				channel->SetMode('C',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleNoCTCP : public Module
{

	NoCTCP nc;

 public:

	ModuleNoCTCP()
		: nc(this)
	{
		if (!ServerInstance->Modes->AddMode(&nc))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	virtual ~ModuleNoCTCP()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for unreal-style channel mode +C", VF_COMMON | VF_VENDOR, API_VERSION);
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

			if (CHANOPS_EXEMPT('C') && c->GetPrefixValue(user) == OP_VALUE)
			{
				return MOD_RES_PASSTHRU;
			}

			if (!c->GetExtBanStatus(user, 'C').check(!c->IsModeSet('C')))
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
