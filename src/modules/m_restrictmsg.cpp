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

/* $ModDesc: Forbids users from messaging each other. Users may still message opers and opers may message other opers. */


class ModuleRestrictMsg : public Module
{

 public:

	ModuleRestrictMsg()
			{

		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}


	virtual ModResult OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if ((target_type == TYPE_USER) && (IS_LOCAL(user)))
		{
			User* u = (User*)dest;

			// message allowed if:
			// (1) the sender is opered
			// (2) the recipient is opered
			// anything else, blocked.
			if (IS_OPER(u) || IS_OPER(user))
			{
				return MOD_RES_PASSTHRU;
			}
			user->WriteNumeric(ERR_CANTSENDTOUSER, "%s %s :You are not permitted to send private messages to this user",user->nick.c_str(),u->nick.c_str());
			return MOD_RES_DENY;
		}

		// however, we must allow channel messages...
		return MOD_RES_PASSTHRU;
	}

	virtual ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return this->OnUserPreMessage(user,dest,target_type,text,status,exempt_list);
	}

	virtual ~ModuleRestrictMsg()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Forbids users from messaging each other. Users may still message opers and opers may message other opers.",VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(ModuleRestrictMsg)
