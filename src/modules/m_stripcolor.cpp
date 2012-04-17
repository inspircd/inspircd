/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Provides channel +S mode (strip ansi color) */

/** Handles channel mode +S
 */
class ChannelStripColor : public SimpleChannelModeHandler
{
 public:
	ChannelStripColor(InspIRCd* Instance) : SimpleChannelModeHandler(Instance, 'S') { }
};

/** Handles user mode +S
 */
class UserStripColor : public SimpleUserModeHandler
{
 public:
	UserStripColor(InspIRCd* Instance) : SimpleUserModeHandler(Instance, 'S') { }
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

		if (!ServerInstance->Modes->AddMode(usc) || !ServerInstance->Modes->AddMode(csc))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	virtual ~ModuleStripColor()
	{
		ServerInstance->Modes->DelMode(usc);
		ServerInstance->Modes->DelMode(csc);
		delete usc;
		delete csc;
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('S');
	}

	virtual void ReplaceLine(std::string &sentence)
	{
		/* refactor this completely due to SQUIT bug since the old code would strip last char and replace with \0 --peavey */
		int seq = 0;
		std::string::iterator i,safei;
 		for (i = sentence.begin(); i != sentence.end();)
		{
			if ((*i == 3))
				seq = 1;
			else if (seq && (( ((*i >= '0') && (*i <= '9')) || (*i == ',') ) ))
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
				if (i != sentence.begin())
				{
					safei = i;
					--i;
					sentence.erase(safei);
					++i;
				}
				else
				{
					sentence.erase(i);
					i = sentence.begin();
				}
			}
			else
				++i;
		}
	}

	virtual int OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (!IS_LOCAL(user))
			return 0;

		bool active = false;
		if (target_type == TYPE_USER)
		{
			User* t = (User*)dest;
			active = t->IsModeSet('S');
		}
		else if (target_type == TYPE_CHANNEL)
		{
			Channel* t = (Channel*)dest;

			// check if we allow ops to bypass filtering, if we do, check if they're opped accordingly.
			// note: short circut logic here, don't wreck it. -- w00t
			if (CHANOPS_EXEMPT(ServerInstance, 'S') && t->GetStatus(user) == STATUS_OP)
			{
				return 0;
			}

			active = t->IsModeSet('S') || t->GetExtBanStatus(user, 'S') < 0;
		}

		if (active)
		{
			this->ReplaceLine(text);
		}

		return 0;
	}

	virtual int OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user,dest,target_type,text,status,exempt_list);
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(ModuleStripColor)
