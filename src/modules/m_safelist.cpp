/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

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
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnBufferFlushed, I_OnPreCommand, I_OnCleanup, I_OnUserQuit, I_On005Numeric, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 6);
	}

	virtual ~ModuleSafeList()
	{
	}

	virtual void OnRehash(User* user)
	{
		ConfigReader MyConf(ServerInstance);
		ThrottleSecs = MyConf.ReadInteger("safelist", "throttle", "60", 0, true);
		LimitList = MyConf.ReadInteger("safelist", "maxlisters", "50", 0, true);
		ServerNameSize = strlen(ServerInstance->Config->ServerName) + 4;
		global_listing = 0;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$",VF_VENDOR,API_VERSION);
	}


	/*
	 * OnPreCommand()
	 *   Intercept the LIST command.
	 */
	virtual int OnPreCommand(std::string &command, std::vector<std::string> &parameters, User *user, bool validated, const std::string &original_line)
	{
		/* If the command doesnt appear to be valid, we dont want to mess with it. */
		if (!validated)
			return 0;

		if (command == "LIST")
		{
			return this->HandleList(parameters, user);
		}
		return 0;
	}

	/*
	 * HandleList()
	 *   Handle (override) the LIST command.
	 */
	int HandleList(const std::vector<std::string> &parameters, User* user)
	{
		int pcnt = parameters.size();
		int minusers = 0, maxusers = 0;

		if (global_listing >= LimitList && !IS_OPER(user))
		{
			user->WriteServ("NOTICE %s :*** Server load is currently too heavy. Please try again later.", user->nick.c_str());
			user->WriteNumeric(321, "%s Channel :Users Name",user->nick.c_str());
			user->WriteNumeric(323, "%s :End of channel list.",user->nick.c_str());
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
			if (parameters[0][0] == '<')
			{
				maxusers = atoi(parameters[0].c_str()+1);
				ServerInstance->Logs->Log("m_safelist",DEBUG,"Max users: %d", maxusers);
				pcnt = 0;
			}
			else if (parameters[0][0] == '>')
			{
				minusers = atoi(parameters[0].c_str()+1);
				ServerInstance->Logs->Log("m_safelist",DEBUG,"Min users: %d", minusers);
				pcnt = 0;
			}
		}

		time_t* last_list_time;
		user->GetExt("safelist_last", last_list_time);
		if (last_list_time)
		{
			if (ServerInstance->Time() < (*last_list_time)+ThrottleSecs)
			{
				user->WriteServ("NOTICE %s :*** Woah there, slow down a little, you can't /LIST so often!",user->nick.c_str());
				user->WriteNumeric(321, "%s Channel :Users Name",user->nick.c_str());
				user->WriteNumeric(323, "%s :End of channel list.",user->nick.c_str());
				return 1;
			}

			delete last_list_time;
			user->Shrink("safelist_last");
		}


		/*
		 * start at channel 0! ;)
		 */
		ld = new ListData(0,ServerInstance->Time(), (pcnt && (parameters[0][0] != '<' && parameters[0][0] != '>')) ? parameters[0] : "*", minusers, maxusers);
		user->Extend("safelist_cache", ld);

		time_t* llt = new time_t;
		*llt = ServerInstance->Time();
		user->Extend("safelist_last", llt);

		user->WriteNumeric(321, "%s Channel :Users Name",user->nick.c_str());

		global_listing++;

		return 1;
	}

	virtual void OnBufferFlushed(User* user)
	{
		char buffer[MAXBUF];
		ListData* ld;
		if (user->GetExt("safelist_cache", ld))
		{
			Channel* chan = NULL;
			unsigned long amount_sent = 0;
			do
			{
				chan = ServerInstance->GetChannelIndex(ld->list_position);
				bool is_special = (chan && (chan->HasUser(user) || user->HasPrivPermission("channels/auspex")));
				long users = chan ? chan->GetUserCounter() : 0;

				bool too_few = (ld->minusers && (users <= ld->minusers));
				bool too_many = (ld->maxusers && (users >= ld->maxusers));

				if (chan && (too_many || too_few))
				{
					ld->list_position++;
					continue;
				}

				if (chan)
				{
					bool display = (InspIRCd::Match(chan->name, ld->glob) || (!chan->topic.empty() && InspIRCd::Match(chan->topic, ld->glob)));

					if (!users || !display)
					{
						ld->list_position++;
						continue;
					}

					/* +s, not in chan / not got channels/auspex */
					if (chan->IsModeSet('s') && !is_special)
					{
						ld->list_position++;
						continue;
					}

					if (chan->IsModeSet('p') && !is_special)
					{
						/* Channel is +p and user is outside/not privileged */
						int counter = snprintf(buffer, MAXBUF, "322 %s * %ld :", user->nick.c_str(), users);
						amount_sent += counter + ServerNameSize;
						user->WriteServ(std::string(buffer));
					}
					else
					{
						/* User is in the channel/privileged, channel is not +s */
						int counter = snprintf(buffer, MAXBUF, "322 %s %s %ld :[+%s] %s", user->nick.c_str(), chan->name.c_str(), users, chan->ChanModes(is_special), chan->topic.c_str());
						amount_sent += counter + ServerNameSize;
						user->WriteServ(std::string(buffer));
					}
				}
				else
				{
					if (!ld->list_ended)
					{
						ld->list_ended = true;
						user->WriteNumeric(323, "%s :End of channel list.",user->nick.c_str());
					}
				}

				ld->list_position++;
			}
			while ((chan != NULL) && (amount_sent < (user->MyClass->GetSendqMax() / 4)));
			if (ld->list_ended)
			{
				user->Shrink("safelist_cache");
				delete ld;
				global_listing--;
			}
		}
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		if(target_type == TYPE_USER)
		{
			User* u = (User*)item;
			ListData* ld;
			u->GetExt("safelist_cache", ld);
			if (ld)
			{
				u->Shrink("safelist_cache");
				delete ld;
				global_listing--;
			}
			time_t* last_list_time;
			u->GetExt("safelist_last", last_list_time);
			if (last_list_time)
			{
				delete last_list_time;
				u->Shrink("safelist_last");
			}
		}
	}

	virtual void On005Numeric(std::string &output)
	{
		output.append(" SAFELIST");
	}

	virtual void OnUserQuit(User* user, const std::string &message, const std::string &oper_message)
	{
		this->OnCleanup(TYPE_USER,user);
	}

};

MODULE_INIT(ModuleSafeList)
