/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h" 
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "wildcard.h"

/** Holds a users m_safelist state
 */
class ListData : public classbase
{
 public:
	long list_start;
	long list_position;
	bool list_ended;
	const std::string glob;
	int minusers;
	int maxusers;

	ListData() : list_start(0), list_position(0), list_ended(false) {};
	ListData(long pos, time_t t, const std::string &pattern, int mi, int ma) : list_start(t), list_position(pos), list_ended(false), glob(pattern), minusers(mi), maxusers(ma) {};
};

/* $ModDesc: A module overriding /list, and making it safe - stop those sendq problems. */

class ModuleSafeList : public Module
{
	time_t ThrottleSecs;
	size_t ServerNameSize;
	int global_listing;
	int LimitList;
 public:
	ModuleSafeList(InspIRCd* Me) : Module(Me)
	{
		OnRehash(NULL, "");
	}
 
	virtual ~ModuleSafeList()
	{
	}

	virtual void OnRehash(userrec* user, const std::string &parameter)
	{
		ConfigReader MyConf(ServerInstance);
		ThrottleSecs = MyConf.ReadInteger("safelist", "throttle", "60", 0, true);
		LimitList = MyConf.ReadInteger("safelist", "maxlisters", "50", 0, true);
		ServerNameSize = strlen(ServerInstance->Config->ServerName) + 4;
		global_listing = 0;
	}
 
	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_VENDOR,API_VERSION);
	}
 
	void Implements(char* List)
	{
		List[I_OnBufferFlushed] = List[I_OnPreCommand] = List[I_OnCleanup] = List[I_OnUserQuit] = List[I_On005Numeric] = List[I_OnRehash] = 1;
	}

	/*
	 * OnPreCommand()
	 *   Intercept the LIST command.
	 */ 
	virtual int OnPreCommand(const std::string &command, const char** parameters, int pcnt, userrec *user, bool validated, const std::string &original_line)
	{
		/* If the command doesnt appear to be valid, we dont want to mess with it. */
		if (!validated)
			return 0;
 
		if (command == "LIST")
		{
			return this->HandleList(parameters, pcnt, user);
		}
		return 0;
	}
	
	/*
	 * HandleList()
	 *   Handle (override) the LIST command.
	 */
	int HandleList(const char** parameters, int pcnt, userrec* user)
	{
		int minusers = 0, maxusers = 0;

		if (global_listing >= LimitList)
		{
			user->WriteServ("NOTICE %s :*** Server load is currently too heavy. Please try again later.", user->nick);
			user->WriteServ("321 %s Channel :Users Name",user->nick);
			user->WriteServ("323 %s :End of channel list.",user->nick);
			return 1;
		}

		/* First, let's check if the user is currently /list'ing */
		ListData *ld;
		user->GetExt("safelist_cache", ld);
 
		if (ld)
		{
			/* user is already /list'ing, we don't want to do shit. */
			return 1;
		}

		/* Work around mIRC suckyness. YOU SUCK, KHALED! */
		if (pcnt == 1)
		{
			if (*parameters[0] == '<')
			{
				maxusers = atoi(parameters[0]+1);
				ServerInstance->Log(DEBUG,"Max users: %d", maxusers);
				pcnt = 0;
			}
			else if (*parameters[0] == '>')
			{
				minusers = atoi(parameters[0]+1);
				ServerInstance->Log(DEBUG,"Min users: %d", minusers);
				pcnt = 0;
			}
		}

		time_t* last_list_time;
		user->GetExt("safelist_last", last_list_time);
		if (last_list_time)
		{
			if (ServerInstance->Time() < (*last_list_time)+ThrottleSecs)
			{
				user->WriteServ("NOTICE %s :*** Woah there, slow down a little, you can't /LIST so often!",user->nick);
				user->WriteServ("321 %s Channel :Users Name",user->nick);
				user->WriteServ("323 %s :End of channel list.",user->nick);
				return 1;
			}

			DELETE(last_list_time);
			user->Shrink("safelist_last");
		}

 
		/*
		 * start at channel 0! ;)
		 */
		ld = new ListData(0,ServerInstance->Time(), pcnt ? parameters[0] : "*", minusers, maxusers);
		user->Extend("safelist_cache", ld);

		time_t* llt = new time_t;
		*llt = ServerInstance->Time();
		user->Extend("safelist_last", llt);

		user->WriteServ("321 %s Channel :Users Name",user->nick);

		global_listing++;

		return 1;
	}

	virtual void OnBufferFlushed(userrec* user)
	{
		char buffer[MAXBUF];
		ListData* ld;
		if (user->GetExt("safelist_cache", ld))
		{
			chanrec* chan = NULL;
			long amount_sent = 0;
			do
			{
				chan = ServerInstance->GetChannelIndex(ld->list_position);
				bool has_user = (chan && chan->HasUser(user));
				long users = chan ? chan->GetUserCounter() : 0;

				bool too_few = (ld->minusers && (users <= ld->minusers));
				bool too_many = (ld->maxusers && (users >= ld->maxusers));

				if (chan && (too_many || too_few))
				{
					ld->list_position++;
					continue;
				}

				if ((chan) && (chan->modes[CM_PRIVATE]))
				{
					bool display = (match(chan->name, ld->glob.c_str()) || (*chan->topic && match(chan->topic, ld->glob.c_str())));
					if ((users) && (display))
					{
						int counter = snprintf(buffer, MAXBUF, "322 %s *", user->nick);
						amount_sent += counter + ServerNameSize;
						user->WriteServ(std::string(buffer));
					}
				}
				else if ((chan) && (((!(chan->modes[CM_PRIVATE])) && (!(chan->modes[CM_SECRET]))) || (has_user)))
				{
					bool display = (match(chan->name, ld->glob.c_str()) || (*chan->topic && match(chan->topic, ld->glob.c_str())));
					if ((users) && (display))
					{
						int counter = snprintf(buffer, MAXBUF, "322 %s %s %ld :[+%s] %s",user->nick, chan->name, users, chan->ChanModes(has_user), chan->topic);
						amount_sent += counter + ServerNameSize;
						user->WriteServ(std::string(buffer));
					}
				}
				else
				{
					if (!chan)
					{
						if (!ld->list_ended)
						{
							ld->list_ended = true;
							user->WriteServ("323 %s :End of channel list.",user->nick);
						}
					}
				}
				ld->list_position++;
			}
			while ((chan != NULL) && (amount_sent < (user->sendqmax / 4)));
			if (ld->list_ended)
			{
				user->Shrink("safelist_cache");
				DELETE(ld);
				global_listing--;
			}
		}
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		if(target_type == TYPE_USER)
		{
			userrec* u = (userrec*)item;
			ListData* ld;
			u->GetExt("safelist_cache", ld);
			if (ld)
			{
				u->Shrink("safelist_cache");
				DELETE(ld);
				global_listing--;
			}
			time_t* last_list_time;
			u->GetExt("safelist_last", last_list_time);
			if (last_list_time)
			{
				DELETE(last_list_time);
				u->Shrink("safelist_last");
			}
		}
	}

	virtual void On005Numeric(std::string &output)
	{
		output.append(" SAFELIST");
	}

	virtual void OnUserQuit(userrec* user, const std::string &message, const std::string &oper_message)
	{
		this->OnCleanup(TYPE_USER,user);
	}

};

MODULE_INIT(ModuleSafeList)
