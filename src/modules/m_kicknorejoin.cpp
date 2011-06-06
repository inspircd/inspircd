/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Provides channel mode +J (delay rejoin after kick) */

typedef std::map<std::string, time_t> delaylist;

/** Handles channel mode +J
 */
class KickRejoin : public ParamChannelModeHandler
{
 public:
	SimpleExtItem<delaylist> ext;
	KickRejoin(Module* Creator) : ParamChannelModeHandler(Creator, "kicknorejoin", 'J'), ext(EXTENSIBLE_CHANNEL, "norejoinusers", Creator)
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

	ModuleKickNoRejoin() : kr(this) {}

	void init()
	{
		ServerInstance->Modules->AddService(kr);
		ServerInstance->Extensions.Register(&kr.ext);
		Implementation eventlist[] = { I_OnCheckJoin, I_OnGarbageCollect, I_OnUserKick };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void OnCheckJoin(ChannelPermissionData& join)
	{
		if (!join.chan || join.result != MOD_RES_PASSTHRU)
			return;
		const delaylist* dl = kr.ext.get(join.chan);
		if (!dl)
			return;

		delaylist::const_iterator iter = dl->find(join.user->uuid);
		if(iter != dl->end() && iter->second > ServerInstance->Time())
		{
			join.ErrorNumeric(ERR_DELAYREJOIN, "%s :You must wait %s seconds after being kicked to rejoin (+J)",
				join.chan->name.c_str(), join.chan->GetModeParameter(&kr).c_str());
			join.result = MOD_RES_DENY;
		}
	}

	void OnGarbageCollect()
	{
		for (chan_hash::const_iterator i = ServerInstance->chanlist->begin(); i != ServerInstance->chanlist->end(); ++i)
		{
			delaylist* dl = kr.ext.get(i->second);
			if (!dl)
				continue;

			for (delaylist::iterator iter = dl->begin(); iter != dl->end();)
			{
				if (iter->second <= ServerInstance->Time())
					dl->erase(iter++);
				else
					++iter;
			}

			if (dl->empty())
				kr.ext.unset(i->second);
		}
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
			(*dl)[memb->user->uuid] = ServerInstance->Time() + atoi(memb->chan->GetModeParameter(&kr).c_str());
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
