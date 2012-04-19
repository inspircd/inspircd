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

/* $ModDesc: Adds user mode +c, which if set, users must be on a common channel with you to private message you */

/** Handles user mode +c
 */
class PrivacyMode : public ModeHandler
{
 public:
	PrivacyMode(InspIRCd* Instance) : ModeHandler(Instance, 'c', 0, 0, false, MODETYPE_USER, false) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool)
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
	PrivacyMode* pm;
 public:
	ModulePrivacyMode(InspIRCd* Me) : Module(Me)
	{
		pm = new PrivacyMode(ServerInstance);
		if (!ServerInstance->Modes->AddMode(pm))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}


	virtual ~ModulePrivacyMode()
	{
		ServerInstance->Modes->DelMode(pm);
		delete pm;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON|VF_VENDOR, API_VERSION);
	}

	virtual int OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type == TYPE_USER)
		{
			User* t = (User*)dest;
			if (!IS_OPER(user) && (t->IsModeSet('c')) && (!ServerInstance->ULine(user->server)) && !user->SharesChannelWith(t))
			{
				user->WriteNumeric(ERR_CANTSENDTOUSER, "%s %s :You are not permitted to send private messages to this user (+c set)", user->nick.c_str(), t->nick.c_str());
				return 1;
			}
		}
		return 0;
	}

	virtual int OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user, dest, target_type, text, status, exempt_list);
	}
};


MODULE_INIT(ModulePrivacyMode)
