/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *		       E-mail:
 *		<brain@chatspike.net>
 *	   	  <Craig@chatspike.net>
 *						<omster@gmail.com>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */
 
/* <3's to Lauren for original info on mIRC's weird behaviour */

#include <string>
#include <time.h>
#include "inspircd_config.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"
#include "hashcomp.h"

/* $ModDesc: Attempt to block /amsg, at least some of the irritating mIRC scripts. */

extern time_t TIME;

enum BlockAction { IBLOCK_KILL, IBLOCK_KILLOPERS, IBLOCK_NOTICE, IBLOCK_NOTICEOPERS, IBLOCK_SILENT };

class BlockedMessage
{
public:
	std::string message;
	irc::string target;
	time_t sent;
	
	BlockedMessage(std::string msg, irc::string tgt, time_t when) : message(msg), target(tgt), sent(when)
	{
	}
};

class ModuleBlockAmsg : public Module
{
	Server* Srv;
	int ForgetDelay;
	BlockAction action;
public:
	ModuleBlockAmsg(Server* Me) : Module::Module(Me)
	{
		Srv = Me;
		this->OnRehash("");
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = List[I_OnPreCommand] = List[I_OnCleanup] = 1;
	}
	
	virtual ~ModuleBlockAmsg()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_VENDOR);
	}
	
	virtual void OnRehash(const std::string &parameter)
	{
		ConfigReader* Conf = new ConfigReader;
		
		ForgetDelay = Conf->ReadInteger("blockamsg", "delay", 0, false);
		if(Conf->GetError() == CONF_VALUE_NOT_FOUND)
			ForgetDelay = -1;
			
		std::string act = Conf->ReadValue("blockamsg", "action", 0);
		
		if(act == "notice")
			action = IBLOCK_NOTICE;
		else if(act == "noticeopers")
			action = IBLOCK_NOTICEOPERS;
		else if(act == "silent")
			action = IBLOCK_SILENT;
		else if(act == "kill")
			action = IBLOCK_KILL;
		else
			action = IBLOCK_KILLOPERS;

		delete Conf;
	}

	virtual int OnPreCommand(const std::string &command, char **parameters, int pcnt, userrec *user, bool validated)
	{
		// Don't do anything with unregistered users, or remote ones.
		if(!user || (user->registered != 7) || !IS_LOCAL(user))
			return 0;
			
		// We want case insensitive command comparison.
		// Add std::string contructor for irc::string :x
		irc::string cmd = command.c_str();
		
		if(validated && (cmd == "PRIVMSG" || cmd == "NOTICE") && (pcnt >= 2))
		{
			// parameters[0] should have the target(s) in it.
			// I think it will be faster to first check if there are any commas, and if there are then try and parse it out.
			// Most messages have a single target so...
			
			int targets = 1;
			int userchans = 0;
			
			// Decrement if the first target wasn't a channel.
			if(*parameters[0] != '#')
				targets--;
			
			for(char* c = parameters[0]; *c; c++)
				if((*c == ',') && *(c+1) && (*(c+1) == '#'))
					targets++;
					
			for(std::vector<ucrec*>::iterator f = user->chans.begin(); f != user->chans.end(); f++)
				if(((ucrec*)(*f))->channel)
					userchans++;

			// Check that this message wasn't already sent within a few seconds.
			BlockedMessage* m = (BlockedMessage*)user->GetExt("amsgblock");
			
			// If the message is identical and within the time.
			// We check the target is *not* identical, that'd straying into the realms of flood control. Which isn't what we're doing...
			// OR
			// The number of target channels is equal to the number of channels the sender is on..a little suspicious.
			// Check it's more than 1 too, or else users on one channel would have fun.
			if((m && (m->message == parameters[1]) && (m->target != parameters[0]) && (ForgetDelay != -1) && (m->sent >= TIME-ForgetDelay)) || ((targets > 1) && (targets == userchans)))
			{
				// Block it...
				if(action == IBLOCK_KILLOPERS || action == IBLOCK_NOTICEOPERS)
					WriteOpers("*** %s had an /amsg or /ame denied", user->nick);

				if(action == IBLOCK_KILL || action == IBLOCK_KILLOPERS)
					Srv->QuitUser(user, "Global message (/amsg or /ame) detected");
				else if(action == IBLOCK_NOTICE || action == IBLOCK_NOTICEOPERS)
					WriteServ(user->fd, "NOTICE %s :Global message (/amsg or /ame) detected", user->nick);
									
				return 1;
			}
			
			if(m)
			{
				// If there's already a BlockedMessage allocated, use it.
				m->message = parameters[1];
				m->target = parameters[0];
				m->sent = TIME;
			}
			else
			{
				m = new BlockedMessage(parameters[1], parameters[0], TIME);
				user->Extend("amsgblock", (char*)m);
			}
		}					
		return 0;
	}
	
	void OnCleanup(int target_type, void* item)
	{
		if(target_type == TYPE_USER)
		{
			userrec* user = (userrec*)item;
			BlockedMessage* m = (BlockedMessage*)user->GetExt("amsgblock");
			if(m)
			{
				delete m;
				user->Shrink("amsgblock");
			}
		}
	}
};


class ModuleBlockAmsgFactory : public ModuleFactory
{
	public:
		ModuleBlockAmsgFactory()
		{
		}
	
		~ModuleBlockAmsgFactory()
		{
		}
	
		virtual Module * CreateModule(Server* Me)
		{
			return new ModuleBlockAmsg(Me);
		}
};


extern "C" void * init_module( void )
{
	return new ModuleBlockAmsgFactory;
}
