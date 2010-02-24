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

/* $ModDesc: Adds user mode +c, which if set, users must be on a common channel with you to private message you */

/** Handles user mode +c
 */
class PrivacyMode : public ModeHandler
{
 public:
	PrivacyMode(Module* Creator) : ModeHandler(Creator, "deaf_commonchan", 'c', PARAM_NONE, MODETYPE_USER) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!dest->IsModeSet('c'))
			{
				dest->SetMode('c',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('c'))
			{
				dest->SetMode('c',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModulePrivacyMode : public Module
{
	PrivacyMode pm;
 public:
	ModulePrivacyMode() : pm(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(pm);
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}


	virtual ~ModulePrivacyMode()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Adds user mode +c, which if set, users must be on a common channel with you to private message you", VF_VENDOR);
	}

	virtual ModResult OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type == TYPE_USER)
		{
			User* t = (User*)dest;
			if (!IS_OPER(user) && (t->IsModeSet('c')) && (!ServerInstance->ULine(user->server)) && !user->SharesChannelWith(t))
			{
				user->WriteNumeric(ERR_CANTSENDTOUSER, "%s %s :You are not permitted to send private messages to this user (+c set)", user->nick.c_str(), t->nick.c_str());
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	virtual ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user, dest, target_type, text, status, exempt_list);
	}
};


MODULE_INIT(ModulePrivacyMode)
