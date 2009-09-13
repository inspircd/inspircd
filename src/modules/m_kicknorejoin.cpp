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

/* $ModDesc: Provides channel mode +J (delay rejoin after kick) */

static inline int strtoint(const std::string &str)
{
	std::istringstream ss(str);
	int result;
	ss >> result;
	return result;
}

typedef std::map<User*, time_t> delaylist;

/** Handles channel mode +J
 */
class KickRejoin : public ModeHandler
{
 public:
	SimpleExtItem<delaylist> ext;
	KickRejoin(InspIRCd* Instance, Module* Creator) : ModeHandler(Creator, 'J', PARAM_SETONLY, MODETYPE_CHANNEL),
		ext("norejoinusers", Creator) { }

	ModePair ModeSet(User* source, User* dest, Channel* channel, const std::string &parameter)
	{
		if (channel->IsModeSet('J'))
			return std::make_pair(true, channel->GetModeParameter('J'));
		else
			return std::make_pair(false, parameter);
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (!adding)
		{
			ext.unset(channel);

			if (!channel->IsModeSet('J'))
			{
				return MODEACTION_DENY;
			}
			else
			{
				channel->SetModeParam('J', "");
				return MODEACTION_ALLOW;
			}
		}
		else if (atoi(parameter.c_str()) > 0)
		{
			if (!channel->IsModeSet('J'))
			{
				parameter = ConvToStr(atoi(parameter.c_str()));
				channel->SetModeParam('J', parameter);
				return MODEACTION_ALLOW;
			}
			else
			{
				std::string cur_param = channel->GetModeParameter('J');
				if (cur_param == parameter)
				{
					// mode params match, don't change mode
					return MODEACTION_DENY;
				}
				else
				{
					// new mode param, replace old with new
					parameter = ConvToStr(atoi(parameter.c_str()));
					if (parameter != "0")
					{
						channel->SetModeParam('J', parameter);
						return MODEACTION_ALLOW;
					}
					else
					{
						/* Fix to jamie's fix, dont allow +J 0 on the new value! */
						return MODEACTION_DENY;
					}
				}
			}
		}
		else
		{
			return MODEACTION_DENY;
		}
	}
};

class ModuleKickNoRejoin : public Module
{
	KickRejoin kr;

public:

	ModuleKickNoRejoin(InspIRCd* Me)
		: Module(Me), kr(Me, this)
	{
		if (!ServerInstance->Modes->AddMode(&kr))
			throw ModuleException("Could not add new modes!");
		Extensible::Register(&kr.ext);
		Implementation eventlist[] = { I_OnUserPreJoin, I_OnUserKick };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	ModResult OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		if (chan)
		{
			delaylist* dl = kr.ext.get(chan);
			if (dl)
			{
				std::vector<User*> itemstoremove;

				for (delaylist::iterator iter = dl->begin(); iter != dl->end(); iter++)
				{
					if (iter->second > ServerInstance->Time())
					{
						if (iter->first == user)
						{
							user->WriteNumeric(ERR_DELAYREJOIN, "%s %s :You must wait %s seconds after being kicked to rejoin (+J)", user->nick.c_str(), chan->name.c_str(), chan->GetModeParameter('J').c_str());
							return MOD_RES_DENY;
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
					kr.ext.unset(chan);
			}
		}
		return MOD_RES_PASSTHRU;
	}

	void OnUserKick(User* source, Membership* memb, const std::string &reason, CUList& excepts)
	{
		if (memb->chan->IsModeSet('J') && (source != memb->user))
		{
			delaylist* dl = kr.ext.get(memb->chan);
			if (dl)
			{
				dl = new delaylist;
				kr.ext.set(memb->chan, dl);
			}
			(*dl)[memb->user] = ServerInstance->Time() + strtoint(memb->chan->GetModeParameter('J'));
		}
	}

	~ModuleKickNoRejoin()
	{
		ServerInstance->Modes->DelMode(&kr);
	}

	Version GetVersion()
	{
		return Version("Channel mode J, kick-no-rejoin", VF_COMMON | VF_VENDOR);
	}
};


MODULE_INIT(ModuleKickNoRejoin)
