/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides channel +S mode (strip ansi colour) */

/** Handles channel mode +S
 */
class ChannelStripColor : public ModeHandler
{
 public:
	ChannelStripColor(InspIRCd* Instance) : ModeHandler(Instance, 'S', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('S'))
			{
				channel->SetMode('S',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('S'))
			{
				channel->SetMode('S',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

/** Handles user mode +S
 */
class UserStripColor : public ModeHandler
{
 public:
	UserStripColor(InspIRCd* Instance) : ModeHandler(Instance, 'S', 0, 0, false, MODETYPE_USER, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		/* Only opers can change other users modes */
		if (source != dest)
			return MODEACTION_DENY;

		if (adding)
		{
			if (!dest->IsModeSet('S'))
			{
				dest->SetMode('S',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('S'))
			{
				dest->SetMode('S',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};


class ModuleStripColor : public Module
{
	bool AllowChanOps;
	ChannelStripColor *csc;
	UserStripColor *usc;
 
 public:
	ModuleStripColor(InspIRCd* Me) : Module(Me)
	{
		usc = new UserStripColor(ServerInstance);
		csc = new ChannelStripColor(ServerInstance);

		if (!ServerInstance->AddMode(usc, 'S') || !ServerInstance->AddMode(csc, 'S'))
			throw ModuleException("Could not add new modes!");
	}

	void Implements(char* List)
	{
		List[I_OnUserPreMessage] = List[I_OnUserPreNotice] = 1;
	}

	virtual ~ModuleStripColor()
	{
		ServerInstance->Modes->DelMode(usc);
		ServerInstance->Modes->DelMode(csc);
		DELETE(usc);
		DELETE(csc);
	}
	
	virtual void ReplaceLine(std::string &sentence)
	{
		/* refactor this completely due to SQUIT bug since the old code would strip last char and replace with \0 --peavey */
		int seq = 0;
		std::string::iterator i,safei;
 		for (i = sentence.begin(); i != sentence.end(); ++i)
		{
			if ((*i == 3))
				seq = 1;
			else if (seq && ( (*i >= '0') && (*i <= '9') || (*i == ',') ) )
			{
				seq++;
				if ( (seq <= 4) && (*i == ',') )
					seq = 1;
				else if (seq > 3)
					seq = 0;
			}
			else
				seq = 0;
			
			if (seq || ((*i == 2) || (*i == 15) || (*i == 22) || (*i == 21) || (*i == 31)))
			{
				safei = i;
				--i;
				sentence.erase(safei);
			}
		}
	}

	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (!IS_LOCAL(user))
			return 0;

		bool active = false;
		if (target_type == TYPE_USER)
		{
			userrec* t = (userrec*)dest;
			active = t->IsModeSet('S');
		}
		else if (target_type == TYPE_CHANNEL)
		{
			chanrec* t = (chanrec*)dest;

			// check if we allow ops to bypass filtering, if we do, check if they're opped accordingly.
			// note: short circut logic here, don't wreck it. -- w00t
			if (!CHANOPS_EXEMPT(ServerInstance, 'S') || CHANOPS_EXEMPT(ServerInstance, 'S') && t->GetStatus(user) != STATUS_OP)
				active = t->IsModeSet('S');
		}

		if (active)
		{
			this->ReplaceLine(text);
		}

		return 0;
	}
	
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user,dest,target_type,text,status,exempt_list);
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
	}
	
};

MODULE_INIT(ModuleStripColor)
