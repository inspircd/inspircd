#include <time.h>
#include <map>
#include <vector>
#include <sstream>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"
#include "inspircd.h"

/* $ModDesc: Provides channel mode +J (delay rejoin after kick) */

extern InspIRCd* ServerInstance;

inline int strtoint(const std::string &str)
{
	std::istringstream ss(str);
	int result;
	ss >> result;
	return result;
}

typedef std::map<userrec*, time_t> delaylist;

class KickRejoin : public ModeHandler
{
 public:
	KickRejoin() : ModeHandler('J', 1, 0, false, MODETYPE_CHANNEL, false) { }

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
		}
		if ((!adding) || (atoi(parameter.c_str()) > 0))
		{
			parameter = ConvToStr(atoi(parameter.c_str()));
			channel->SetModeParam('J', parameter.c_str(), adding);
			channel->SetMode('J', adding);
			return MODEACTION_ALLOW;
		}
		else
		{
			return MODEACTION_DENY;
		}
	}
};

class ModuleKickNoRejoin : public Module
{
	Server *Srv;
	KickRejoin* kr;
	
public:
 
	ModuleKickNoRejoin(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		kr = new KickRejoin();
		Srv->AddMode(kr, 'J');
	}

	virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname)
	{
		if (chan)
		{
			delaylist* dl;
			if (chan->GetExt("norejoinusers", dl))
			{
				log(DEBUG, "m_kicknorejoin.so: got delay list, iterating over it");
				std::vector<userrec*> itemstoremove;
			
				for (delaylist::iterator iter = dl->begin(); iter != dl->end(); iter++)
				{
					log(DEBUG, "m_kicknorejoin.so:\t[%s] => %d", iter->first->nick, iter->second);
					if (iter->second > time(NULL))
					{
						log(DEBUG, "m_kicknorejoin.so: still inside time slot");
						if (iter->first == user)					
						{
							log(DEBUG, "m_kicknorejoin.so: and we have the right user");
							user->WriteServ( "495 %s %s :You cannot rejoin this channel yet after being kicked (+J)", user->nick, chan->name);
							return 1;
						}
					}
					else
					{
						// Expired record, remove.
						log(DEBUG, "m_kicknorejoin.so: record expired");
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
		
	virtual void OnUserKick(userrec* source, userrec* user, chanrec* chan, const std::string &reason)
	{
		if (chan->IsModeSet('J') && (source != user))
		{
			delaylist* dl;
			if (!chan->GetExt("norejoinusers", dl))
			{
				dl = new delaylist;
				chan->Extend("norejoinusers", dl);
			}
			
			log(DEBUG, "m_kicknorejoin.so: setting record for %s, %d second delay", user->nick, strtoint(chan->GetModeParameter('J')));
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
		List[I_OnCleanup] = List[I_On005Numeric] = List[I_OnChannelDelete] = List[I_OnUserPreJoin] = List[I_OnUserKick] = 1;
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->ModeGrok->InsertMode(output, "J", 3);
	}

	virtual ~ModuleKickNoRejoin()
	{
		DELETE(kr);
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_STATIC | VF_VENDOR);
	}
};


class ModuleKickNoRejoinFactory : public ModuleFactory
{
 public:
	ModuleKickNoRejoinFactory()
	{
	}
	
	~ModuleKickNoRejoinFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleKickNoRejoin(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleKickNoRejoinFactory;
}

