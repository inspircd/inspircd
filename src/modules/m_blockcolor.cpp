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

/* $ModDesc: Provides support for unreal-style channel mode +c */

/** Handles the +c channel mode
 */
class BlockColor : public SimpleChannelModeHandler
{
 public:
	BlockColor(InspIRCd* Instance, Module* Creator) : SimpleChannelModeHandler(Instance, Creator, 'c') { }
};

class ModuleBlockColour : public Module
{
	bool AllowChanOps;
	BlockColor bc;
 public:

	ModuleBlockColour(InspIRCd* Me) : Module(Me), bc(Me, this)
	{
		if (!ServerInstance->Modes->AddMode(&bc))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 3);
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

			if (CHANOPS_EXEMPT(ServerInstance, 'c') && c->GetPrefixValue(user) == OP_VALUE)
			{
				return MOD_RES_PASSTHRU;
			}

			if (!c->GetExtBanStatus(user, 'c').check(!c->IsModeSet('c')))
			{
				for (std::string::iterator i = text.begin(); i != text.end(); i++)
				{
					switch (*i)
					{
						case 2:
						case 3:
						case 15:
						case 21:
						case 22:
						case 31:
							user->WriteNumeric(404, "%s %s :Can't send colours to channel (+c set)",user->nick.c_str(), c->name.c_str());
							return MOD_RES_DENY;
						break;
					}
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}

	virtual ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user,dest,target_type,text,status,exempt_list);
	}

	virtual ~ModuleBlockColour()
	{
		ServerInstance->Modes->DelMode(&bc);
	}

	virtual Version GetVersion()
	{
		return Version("$Id$",VF_COMMON|VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(ModuleBlockColour)
