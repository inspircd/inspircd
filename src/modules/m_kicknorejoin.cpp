#include <time.h>
#include <map>
#include <vector>
#include <sstream>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Provides channel mode +J (delay rejoin after kick) */

inline int strtoint(const std::string &str)
{
	std::istringstream ss(str);
	int result;
	ss >> result;
	return result;
}

typedef std::map<userrec*, time_t> delaylist;

class ModuleKickNoRejoin : public Module
{
	Server *Srv;
	
public:
 
	ModuleKickNoRejoin(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		
		Srv->AddExtendedMode('J', MT_CHANNEL, false, 1, 0);
	}
	
	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		if ((modechar == 'J') && (type == MT_CHANNEL))
		{
			if (!mode_on)
			{
				// Taking the mode off, we need to clean up.
				chanrec* c = (chanrec*)target;
				
				delaylist* dl = (delaylist*)c->GetExt("norejoinusers");
				
				if (dl)
				{
					delete dl;
					c->Shrink("norejoinusers");
				}
			}
			/* Don't allow negative or 0 +J value */
			return (atoi(params[0].c_str()) > 0);
		}
		return 0;
	}

	virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname)
	{
		if (chan)
		{
			delaylist* dl = (delaylist*)chan->GetExt("norejoinusers");
			log(DEBUG, "m_kicknorejoin.so: tried to grab delay list");
			
			if (dl)
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
							WriteServ(user->fd, "495 %s %s :You cannot rejoin this channel yet after being kicked (+J)", user->nick, chan->name);
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
					delete dl;
					chan->Shrink("norejoinusers");
				}
			}
		}
		return 0;
	}
	
	virtual void OnUserKick(userrec* source, userrec* user, chanrec* chan, std::string reason)
	{
		if (chan->IsCustomModeSet('J') && (source != user))
		{
			delaylist* dl = (delaylist*)chan->GetExt("norejoinusers");
			
			if (!dl)
			{
				dl = new delaylist;
				chan->Extend("norejoinusers", (char*)dl);
			}
			
			log(DEBUG, "m_kicknorejoin.so: setting record for %s, %d second delay", user->nick, strtoint(chan->GetModeParameter('J')));
			(*dl)[user] = time(NULL) + strtoint(chan->GetModeParameter('J'));
		}
	}
	
	virtual void OnChannelDelete(chanrec* chan)
	{
		delaylist* dl = (delaylist*)chan->GetExt("norejoinusers");
		
		if (dl)
		{
			delete dl;
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
		List[I_OnCleanup] = List[I_On005Numeric] = List[I_OnExtendedMode] = List[I_OnChannelDelete] = List[I_OnUserPreJoin] = List[I_OnUserKick] = 1;
	}

	virtual void On005Numeric(std::string &output)
	{
		InsertMode(output, "J", 3);
	}

	virtual ~ModuleKickNoRejoin()
	{
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

