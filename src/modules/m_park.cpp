/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
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

#include <stdio.h>
#include <string>
#include <vector>
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides support for user parking/unparking */

class awaymsg
{
 public:
	std::string from;
	std::string text;
	time_t tm;
};

class parkedinfo
{
 public:
	std::string nick;
	std::string host;
	time_t parktime;
};

Server *Srv;
typedef std::vector<awaymsg> awaylog;
typedef std::vector<parkedinfo> parkinfo;
parkinfo pinfo;
long ParkMaxTime;
long ConcurrentParks;
long ParkMaxMsgs;
parkedinfo pi;

void handle_park(char **parameters, int pcnt, userrec *user)
{
	/** Parking. easy stuff.
	 *
	 * We duplicate and switch the users file descriptor, so that they can remain forever as a 'ghost'
	 * We then disconnect the real user leaving a controlled ghost in their place :)
	 */
	int othersessions = 0;
	if (pinfo.size())
	        for (parkinfo::iterator j = pinfo.begin(); j != pinfo.end(); j++)
			if (j->host == std::string(user->host))
				othersessions++;
	if (othersessions >= ConcurrentParks)
	{
		Srv->SendServ(user->fd,"927 "+std::string(user->nick)+" :You are already parked up to the maximum number of allowed times.");
	}
	else
	{
		awaylog* aw;
		char msg[MAXBUF];
		long key = abs(random() * 12345);
		snprintf(msg,MAXBUF,"You are now parked. To unpark use /UNPARK %s %d",user->nick,key);
		Srv->UserToPseudo(user,std::string(msg));
		aw = new awaylog;
		user->Extend("park_awaylog",(char*)aw);
		user->Extend("park_key",(char*)key);
		pi.nick = user->nick;
		pi.host = user->host;
		pi.parktime = time(NULL);
		pinfo.push_back(pi);
	}
}

void handle_parkstats(char **parameters, int pcnt, userrec *user)
{
	char status[MAXBUF];
	snprintf(status,MAXBUF,"NOTICE %s :There are a total of %d parked clients on this server, with a maximum of %d parked sessions allowed per user.",user->nick,pinfo.size(),ConcurrentParks);
	Srv->SendServ(user->fd,status);
}

void handle_unpark(char **parameters, int pcnt, userrec *user)
{
	/** Unparking. complicated stuff.
	 *
	 * Unparking is done in several steps:
	 *
	 * (1) Check if the user is parked
	 * (2) Check the key of the user against the one provided
	 * (3) Part the user who issued the command from all of their channels so as not to confuse clients
	 * (4) Remove all the users UMODEs
	 * (5) Duplicate and switch file descriptors on the two users, and disconnect the dead one
	 * (6) Force a nickchange onto the user who issued the command forcing them to change to the parked nick
	 * (7) Force join the user into all the channels the parked nick is currently in (send them localized join and namelist)
	 * (8) Send the user the umodes of their new 'persona'
	 * (9) Spool any awaylog messages to the user
	 *
	 * And there you have it, easy huh (NOT)...
	 */
	userrec* unpark = Srv->FindNick(std::string(parameters[0]));
	if (!unpark)
	{
		WriteServ(user->fd,"942 %s %s :Invalid user specified.",user->nick, parameters[0]);
		return;
	}
	awaylog* awy = (awaylog*)unpark->GetExt("park_awaylog");
	long key = (long)unpark->GetExt("park_key");
	if (!awy)
	{
		WriteServ(user->fd,"943 %s %s :This user is not parked.",user->nick, unpark->nick);
		return;
	}
	if (key == atoi(parameters[1]))
	{
		// first part the user from all chans theyre on, so things dont get messy
	        for (int i = 0; i != MAXCHANS; i++)
	        {
	                if (user->chans[i].channel != NULL)
	                {
	                        if (user->chans[i].channel->name)
	                        {
					Srv->PartUserFromChannel(user,user->chans[i].channel->name,"Unparking");
				}
			}
		}
		// remove all their old modes
		WriteServ(user->fd,"MODE %s -%s",user->nick,user->modes);
		// now, map them to the parked user, while nobody can see :p
		Srv->PseudoToUser(user,unpark,"Unparked to "+std::string(parameters[0]));
		// set all their new modes
		WriteServ(unpark->fd,"MODE %s +%s",unpark->nick,unpark->modes);
		// spool their away log to them
		WriteServ(unpark->fd,"NOTICE %s :*** You are now unparked. You have successfully taken back the nickname and privilages of %s.",unpark->nick,unpark->nick);
		for (awaylog::iterator i = awy->begin(); i != awy->end(); i++)
		{
			char timebuf[MAXBUF];
			tm *timeinfo = localtime(&i->tm);
			strlcpy(timebuf,asctime(timeinfo),MAXBUF);
			timebuf[strlen(timebuf)-1] = '\0';
			WriteServ(unpark->fd,"NOTICE %s :From %s at %s: \2%s\2",unpark->nick,i->from.c_str(),timebuf,i->text.c_str());
		}
		delete awy;
		unpark->Shrink("park_awaylog");
		unpark->Shrink("park_key");
		for (parkinfo::iterator j = pinfo.begin(); j != pinfo.end(); j++)
		{
			if (j->nick == std::string(unpark->nick))
			{
				pinfo.erase(j);
				break;
			}
		}
	}
	else
	{
		Srv->SendServ(user->fd,"928 "+std::string(user->nick)+" :Incorrect park key.");
	}
}


class ModulePark : public Module
{
 protected:
	ConfigReader* Conf;
 public:
	virtual void ReadSettings()
	{
		Conf = new ConfigReader;
		ParkMaxTime = Conf->ReadInteger("park","maxtime",0,true);
		ConcurrentParks = Conf->ReadInteger("park","maxperip",0,true);
		ParkMaxMsgs = Conf->ReadInteger("park","maxmessages",0,true);
		delete Conf;
	}

	ModulePark()
	{
		Srv = new Server;
		pinfo.clear();
		this->ReadSettings();
		Srv->AddCommand("PARK",handle_park,0,0,"m_park.so");
		Srv->AddCommand("UNPARK",handle_unpark,0,2,"m_park.so");
		Srv->AddCommand("PARKSTATS",handle_parkstats,'o',0,"m_park.so");
	}
	
	virtual ~ModulePark()
	{
		delete Srv;
	}

	virtual void OnRehash()
	{
		this->ReadSettings();
	}

        virtual void On005Numeric(std::string &output)
        {
                output = output + std::string(" PARK");
        }

        virtual void OnUserQuit(userrec* user)
        {
                std::string nick = user->nick;
                // track quits in our parked user list
                for (parkinfo::iterator j = pinfo.begin(); j != pinfo.end(); j++)
                {
                        if (j->nick == nick)
                        {
                                pinfo.erase(j);
                                break;
                        }
                }
        }


	virtual void OnPrePrivmsg(userrec* user, userrec* dest, std::string text)
	{
		awaylog* awy = (awaylog*)dest->GetExt("park_awaylog");
		if (awy)
		{
			if (awy->size() <= ParkMaxMsgs)
			{
				awaymsg am;
				am.text = text;
				am.from = user->GetFullHost();
				am.tm = time(NULL);
				awy->push_back(am);
				Srv->SendServ(user->fd,"930 "+std::string(user->nick)+" :User "+std::string(dest->nick)+" is parked. Your message has been stored.");
			}
			else Srv->SendServ(user->fd,"929 "+std::string(user->nick)+" :User "+std::string(dest->nick)+" is parked, but their message queue is full. Message not saved.");
		}
	}

	virtual int OnUserPreNick(userrec* user, std::string newnick)
	{
		// track nickchanges in our parked user list
		// (this isnt too efficient, i'll tidy it up some time)
                for (parkinfo::iterator j = pinfo.begin(); j != pinfo.end(); j++)
                {
                        if (j->nick == std::string(user->nick))
                        {
                                j->nick = newnick;
                                break;
                        }
                }
		return 0;
	}

	virtual void OnBackgroundTimer(time_t curtime)
	{
	        // look for parked clients which have timed out (this needs tidying)
	        if (pinfo.empty())
	                return;
        	bool go_again = true;
	        while (go_again)
	        {
	                go_again = false;
	                for (parkinfo::iterator j = pinfo.begin(); j != pinfo.end(); j++)
	                {
	                        if (time(NULL) >= (j->parktime+ParkMaxTime))
	                        {
	                                userrec* thisnick = Srv->FindNick(j->nick);
					// THIS MUST COME BEFORE THE QuitUser - QuitUser can
					// create a recursive call to OnUserQuit in this module
					// and then corrupt the pointer!
					pinfo.erase(j);
	                                if (thisnick)
	                                        Srv->QuitUser(thisnick,"PARK timeout");
	                                go_again = true;
	                                break;
	                        }
	                }
	        }
	}

	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text)
	{
		if (target_type == TYPE_USER)
		{
			userrec* u = (userrec*)dest;
			OnPrePrivmsg(user,u,text);
			return 1;
		}
		return 0;
	}

        virtual void OnWhois(userrec* src, userrec* dst)
        {
		if (dst->GetExt("park_awaylog"))
	                Srv->SendTo(NULL,src,"335 "+std::string(src->nick)+" "+std::string(dst->nick)+" :is a parked client");
        }
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}
	
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleParkFactory : public ModuleFactory
{
 public:
	ModuleParkFactory()
	{
	}
	
	~ModuleParkFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModulePark;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleParkFactory;
}

