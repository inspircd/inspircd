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

/* $ModDesc: Provides support for unreal-style channel mode +c */

/** Handles the +c channel mode
 */
class BlockColor : public SimpleChannelModeHandler
{
 public:
	BlockColor(Module* Creator) : SimpleChannelModeHandler(Creator, "blockcolor", 'c') { fixed_letter = false; }
};

class ModuleBlockColour : public Module
{
	bool AllowChanOps;
	BlockColor bc;
 public:

	ModuleBlockColour() : bc(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(bc);
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice, I_OnUserPart, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('c');
	}

	virtual ModResult OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if ((target_type == TYPE_CHANNEL) && (IS_LOCAL(user)))
		{
			Channel* c = (Channel*)dest;

			if (ServerInstance->CheckExemption(user,c,"blockcolor") != MOD_RES_ALLOW && !c->GetExtBanStatus(user, 'c').check(!c->IsModeSet(&bc)))
				if (text.find_first_of("\x02\x03\x0f\x15\x16\x1f") != std::string::npos)
				{
					user->WriteNumeric(404, "%s %s :Can't send colours to channel (+c set)",user->nick.c_str(), c->name.c_str());
					return MOD_RES_DENY;
				}
		}
		return MOD_RES_PASSTHRU;
	}

	virtual ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user,dest,target_type,text,status,exempt_list);
	}

	virtual void OnUserPart(Membership* memb, std::string &partmessage, CUList&)
	{
		if (!IS_LOCAL(memb->user))
			return;

		if (ServerInstance->CheckExemption(memb->user,memb->chan,"blockcolor") == MOD_RES_ALLOW)
			return;

		if (!memb->chan->GetExtBanStatus(memb->user, 'c').check(!memb->chan->IsModeSet(&bc)))
			if (partmessage.find_first_of("\x02\x03\x0f\x15\x16\x1f") != std::string::npos)
				partmessage = "";
	}

	virtual ~ModuleBlockColour()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for unreal-style channel mode +c",VF_VENDOR);
	}
};

MODULE_INIT(ModuleBlockColour)
