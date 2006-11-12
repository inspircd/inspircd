/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include "users.h"
#include "channels.h"
#include "modules.h"

#include "inspircd.h"

/* $ModDesc: Restricts banned users in a channel. May not speak, etc. */

class ModuleRestrictBanned : public Module
{
 private:
 public:
	ModuleRestrictBanned(InspIRCd* Me) : Module::Module(Me)
	{
	}
	
	virtual ~ModuleRestrictBanned()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}

	void Implements(char* List)
	{
		List[I_OnLocalTopicChange] = List[I_OnUserPreNick] = List[I_OnUserPreNotice] = List[I_OnUserPreMessage] = 1;
	}

	int CheckRestricted(userrec *user, chanrec *channel, const std::string &action)
	{
		/* aren't local? we don't care. */
		if (!IS_LOCAL(user))
			return 0;

		if (channel->IsBanned(user) && channel->GetStatus(user) < STATUS_VOICE)
		{
			/* banned, boned. drop the message. */
			user->WriteServ("NOTICE "+std::string(user->nick)+" :*** You may not " + action + ", as you are banned on channel " + channel->name);
			return 1;
		}

		return 0;
	}

	virtual int OnUserPreNick(userrec *user, const std::string &newnick)
	{
		/* if they aren't local, we don't care */
		if (!IS_LOCAL(user))
			return 0;

		/* bit of a special case. */
		for (std::vector<ucrec*>::iterator i = user->chans.begin(); i != user->chans.end(); i++)
		{
			if (((ucrec*)(*i))->channel != NULL)
			{
				chanrec *channel = ((ucrec*)(*i))->channel;

				if (CheckRestricted(user, channel, "change your nickname") == 1)
					return 1;
			}
		}

		return 0;
	}

	virtual int OnLocalTopicChange(userrec *user, chanrec *channel, const std::string &topic)
	{
		return CheckRestricted(user, channel, "change the topic");
	}
	
	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		return OnUserPreNotice(user,dest,target_type,text,status);
	}

	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		if (target_type == TYPE_CHANNEL)
		{
			chanrec *channel = (chanrec *)dest;

			return CheckRestricted(user, channel, "message the channel");
		}

		return 0;
	}
};


class ModuleRestrictBannedFactory : public ModuleFactory
{
 public:
	ModuleRestrictBannedFactory()
	{
	}
	
	~ModuleRestrictBannedFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleRestrictBanned(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleRestrictBannedFactory;
}

