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

/* $ModDesc: Provides support for unreal-style channel mode +c */

class NoCTCP : public ModeHandler
{
 public:
	NoCTCP(InspIRCd* Instance) : ModeHandler(Instance, 'C', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
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
	
	NoCTCP* nc;
	
 public:
 
	ModuleNoCTCP(InspIRCd* Me)
		: Module(Me)
	{
		
		nc = new NoCTCP(ServerInstance);
		if (!ServerInstance->AddMode(nc, 'C'))
			throw ModuleException("Could not add new modes!");
	}

	void Implements(char* List)
	{
		List[I_OnUserPreMessage] = List[I_OnUserPreNotice] = 1;
	}

	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreNotice(user,dest,target_type,text,status,exempt_list);
	}
	
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if ((target_type == TYPE_CHANNEL) && (IS_LOCAL(user)))
		{
			chanrec* c = (chanrec*)dest;
			if (c->IsModeSet('C'))
			{
				if ((text.length()) && (text[0] == '\1'))
				{
					if (strncmp(text.c_str(),"\1ACTION ",8))
					{
						user->WriteServ("492 %s %s :Can't send CTCP to channel (+C set)",user->nick, c->name);
						return 1;
					}
				}
			}
		}
		return 0;
	}

	virtual ~ModuleNoCTCP()
	{
		ServerInstance->Modes->DelMode(nc);
		DELETE(nc);
	}

	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_COMMON|VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(ModuleNoCTCP)
