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

/* $ModDesc: Provides channel mode +J (delay rejoin after kick) */

typedef std::map<User*, time_t> delaylist;

/** Handles channel mode +J
 */
class KickRejoin : public ParamChannelModeHandler
{
 public:
	SimpleExtItem<delaylist> ext;
	KickRejoin(Module* Creator) : ParamChannelModeHandler(Creator, "kicknorejoin", 'J'), ext("norejoinusers", Creator)
	{
		fixed_letter = false;
	}

	bool ParamValidate(std::string& parameter)
	{
		int v = atoi(parameter.c_str());
		if (v <= 0)
			return false;
		parameter = ConvToStr(v);
		return true;
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		ModeAction rv = ParamChannelModeHandler::OnModeChange(source, dest, channel, parameter, adding);
		if (rv == MODEACTION_ALLOW && !adding)
			ext.unset(channel);
		return rv;
	}
};

class ModuleKickNoRejoin : public Module
{
	KickRejoin kr;

public:

	ModuleKickNoRejoin()
		: kr(this)
	{
		ServerInstance->Modules->AddService(kr);
		ServerInstance->Extensions.Register(&kr.ext);
		Implementation eventlist[] = { I_OnCheckJoin, I_OnUserKick };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	void OnCheckJoin(ChannelPermissionData& join)
	{
		if (!join.chan || join.result != MOD_RES_PASSTHRU)
			return;
		delaylist* dl = kr.ext.get(join.chan);
		if (!dl)
			return;
		std::vector<User*> itemstoremove;

		for (delaylist::iterator iter = dl->begin(); iter != dl->end(); iter++)
		{
			if (iter->second > ServerInstance->Time())
			{
				if (iter->first == join.user)
				{
					join.ErrorNumeric(ERR_DELAYREJOIN, "%s :You must wait %s seconds after being kicked to rejoin (+J)",
						join.chan->name.c_str(), join.chan->GetModeParameter(&kr).c_str());
					join.result = MOD_RES_DENY;
					return;
				}
			}
			else
			{
				// Expired record, remove.
				itemstoremove.push_back(iter->first);
			}
		}

		for (unsigned int i = 0; i < itemstoremove.size(); i++)
			dl->erase(itemstoremove[i]);

		if (!dl->size())
			kr.ext.unset(join.chan);
	}

	void OnUserKick(User* source, Membership* memb, const std::string &reason, CUList& excepts)
	{
		if (memb->chan->IsModeSet(&kr) && (source != memb->user))
		{
			delaylist* dl = kr.ext.get(memb->chan);
			if (!dl)
			{
				dl = new delaylist;
				kr.ext.set(memb->chan, dl);
			}
			(*dl)[memb->user] = ServerInstance->Time() + atoi(memb->chan->GetModeParameter(&kr).c_str());
		}
	}

	~ModuleKickNoRejoin()
	{
	}

	Version GetVersion()
	{
		return Version("Channel mode to delay rejoin after kick", VF_VENDOR);
	}
};


MODULE_INIT(ModuleKickNoRejoin)
