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
#include <sstream>
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides channel mode +J (delay rejoin after kick) */

inline int strtoint(const std::string &str)
{
	std::istringstream ss(str);
	int result;
	ss >> result;
	return result;
}

typedef std::map<userrec*, time_t> delaylist;

/** Handles channel mode +J
 */
class KickRejoin : public ModeHandler
{
 public:
	KickRejoin(InspIRCd* Instance) : ModeHandler(Instance, 'J', 1, 0, false, MODETYPE_CHANNEL, false) { }

	ModePair ModeSet(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter)
	{
		if (channel->IsModeSet('J'))
			return std::make_pair(true, channel->GetModeParameter('J'));
		else
			return std::make_pair(false, parameter);
	} 

	bool CheckTimeStamp(time_t theirs, time_t ours, const std::string &their_param, const std::string &our_param, chanrec* channel)
	{
		/* When TS is equal, the alphabetically later one wins */
		return (their_param < our_param);
	}
	
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (!adding)
		{
			// Taking the mode off, we need to clean up.
			delaylist* dl;
			
			if (channel->GetExt("norejoinusers", dl))
			{
				DELETE(dl);
				channel->Shrink("norejoinusers");
			}
			
			if (!channel->IsModeSet('J'))
			{
				return MODEACTION_DENY;
			}
			else
			{
				channel->SetMode('J', false);
				return MODEACTION_ALLOW;
			}
		}
		else if (atoi(parameter.c_str()) > 0)
		{
			if (!channel->IsModeSet('J'))
			{
				parameter = ConvToStr(atoi(parameter.c_str()));
				channel->SetModeParam('J', parameter.c_str(), adding);
				channel->SetMode('J', adding);
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
					cur_param = ConvToStr(atoi(cur_param.c_str()));
					if (parameter != "0")
					{
						channel->SetModeParam('J', cur_param.c_str(), false);
						channel->SetModeParam('J', parameter.c_str(), adding);
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
		if (!ServerInstance->AddMode(kr, 'J'))
			throw ModuleException("Could not add new modes!");
	}

	virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname, std::string &privs)
	{
		if (chan)
		{
			delaylist* dl;
			if (chan->GetExt("norejoinusers", dl))
			{
				std::vector<userrec*> itemstoremove;
			
				for (delaylist::iterator iter = dl->begin(); iter != dl->end(); iter++)
				{
					if (iter->second > time(NULL))
					{
						if (iter->first == user)					
						{
							user->WriteServ( "495 %s %s :You cannot rejoin this channel yet after being kicked (+J)", user->nick, chan->name);
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
					DELETE(dl);
					chan->Shrink("norejoinusers");
				}
			}
		}
		return 0;
	}
		
	virtual void OnUserKick(userrec* source, userrec* user, chanrec* chan, const std::string &reason, bool &silent)
	{
		if (chan->IsModeSet('J') && (source != user))
		{
			delaylist* dl;
			if (!chan->GetExt("norejoinusers", dl))
			{
				dl = new delaylist;
				chan->Extend("norejoinusers", dl);
			}
			(*dl)[user] = time(NULL) + strtoint(chan->GetModeParameter('J'));
		}
	}
	
	virtual void OnChannelDelete(chanrec* chan)
	{
		delaylist* dl;
			
		if (chan->GetExt("norejoinusers", dl))
		{
			DELETE(dl);
			chan->Shrink("norejoinusers");
		}
	}
	
	virtual void OnCleanup(int target_type, void* item)
	{
		if(target_type == TYPE_CHANNEL)
			OnChannelDelete((chanrec*)item);
	}

	virtual void Implements(char* List)
	{
		List[I_OnCleanup] = List[I_OnChannelDelete] = List[I_OnUserPreJoin] = List[I_OnUserKick] = 1;
	}

	virtual ~ModuleKickNoRejoin()
	{
		ServerInstance->Modes->DelMode(kr);
		DELETE(kr);
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
	}
};


MODULE_INIT(ModuleKickNoRejoin)
