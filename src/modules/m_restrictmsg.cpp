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
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Forbids users from messaging each other. Users may still message opers and opers may message other opers. */


class ModuleRestrictMsg : public Module
{
	
 public:
 
	ModuleRestrictMsg(InspIRCd* Me)
		: Module(Me)
	{
		
	}

	void Implements(char* List)
	{
		List[I_OnUserPreMessage] = List[I_OnUserPreNotice] = 1;
	}

	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if ((target_type == TYPE_USER) && (IS_LOCAL(user)))
		{
			userrec* u = (userrec*)dest;

			// message allowed if:
			// (1) the sender is opered
			// (2) the recipient is opered
			// anything else, blocked.
			if (IS_OPER(u) || IS_OPER(user))
			{
				return 0;
			}
			user->WriteServ("531 %s %s :You are not permitted to send private messages to this user",user->nick,u->nick);
			return 1;
		}

		// however, we must allow channel messages...
		return 0;
	}

	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return this->OnUserPreMessage(user,dest,target_type,text,status,exempt_list);
	}

	virtual ~ModuleRestrictMsg()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(ModuleRestrictMsg)
