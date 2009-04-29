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

inline int strtoint(const std::string &str)
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
	KickRejoin(InspIRCd* Instance) : ModeHandler(Instance, 'J', 1, 0, false, MODETYPE_CHANNEL, false) { }

	ModePair ModeSet(User* source, User* dest, Channel* channel, const std::string &parameter)
	{
		if (channel->IsModeSet('J'))
			return std::make_pair(true, channel->GetModeParameter('J'));
		else
			return std::make_pair(false, parameter);
	}

	bool CheckTimeStamp(time_t theirs, time_t ours, const std::string &their_param, const std::string &our_param, Channel* channel)
	{
		/* When TS is equal, the alphabetically later one wins */
		return (their_param < our_param);
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool)
	{
		if (!adding)
		{
			// Taking the mode off, we need to clean up.
			delaylist* dl;

			if (channel->GetExt("norejoinusers", dl))
			{
				delete dl;
				channel->Shrink("norejoinusers");
			}

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

	KickRejoin* kr;

public:

	ModuleKickNoRejoin(InspIRCd* Me)
		: Module(Me)
	{

		kr = new KickRejoin(ServerInstance);
		if (!ServerInstance->Modes->AddMode(kr))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnCleanup, I_OnChannelDelete, I_OnUserPreJoin, I_OnUserKick };
		ServerInstance->Modules->Attach(eventlist, this, 4);
	}

	virtual int OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		if (chan)
		{
			delaylist* dl;
			if (chan->GetExt("norejoinusers", dl))
			{
				std::vector<User*> itemstoremove;

				for (delaylist::iterator iter = dl->begin(); iter != dl->end(); iter++)
				{
					if (iter->second > ServerInstance->Time())
					{
						if (iter->first == user)
						{
							user->WriteNumeric(ERR_DELAYREJOIN, "%s %s :You must wait %s seconds after being kicked to rejoin (+J)", user->nick.c_str(), chan->name.c_str(), chan->GetModeParameter('J').c_str());
							return 1;
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
				{
					// Now it's empty..
					delete dl;
					chan->Shrink("norejoinusers");
				}
			}
		}
		return 0;
	}

	virtual void OnUserKick(User* source, User* user, Channel* chan, const std::string &reason, bool &silent)
	{
		if (chan->IsModeSet('J') && (source != user))
		{
			delaylist* dl;
			if (!chan->GetExt("norejoinusers", dl))
			{
				dl = new delaylist;
				chan->Extend("norejoinusers", dl);
			}
			(*dl)[user] = ServerInstance->Time() + strtoint(chan->GetModeParameter('J'));
		}
	}

	virtual void OnChannelDelete(Channel* chan)
	{
		delaylist* dl;

		if (chan->GetExt("norejoinusers", dl))
		{
			delete dl;
			chan->Shrink("norejoinusers");
		}
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		if(target_type == TYPE_CHANNEL)
			OnChannelDelete((Channel*)item);
	}


	virtual ~ModuleKickNoRejoin()
	{
		ServerInstance->Modes->DelMode(kr);
		delete kr;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}
};


MODULE_INIT(ModuleKickNoRejoin)
