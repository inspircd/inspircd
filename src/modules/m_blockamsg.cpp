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
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "hashcomp.h"

/* $ModDesc: Attempt to block /amsg, at least some of the irritating mIRC scripts. */

enum BlockAction { IBLOCK_KILL, IBLOCK_KILLOPERS, IBLOCK_NOTICE, IBLOCK_NOTICEOPERS, IBLOCK_SILENT };
/*	IBLOCK_NOTICE		- Send a notice to the user informing them of what happened.
 *	IBLOCK_NOTICEOPERS	- Send a notice to the user informing them and send an oper notice.
 *	IBLOCK_SILENT		- Generate no output, silently drop messages.
 *	IBLOCK_KILL			- Kill the user with the reason "Global message (/amsg or /ame) detected".
 *	IBLOCK_KILLOPERS	- As above, but send an oper notice as well. This is the default.
 */

/** Holds a blocked message's details
 */
class BlockedMessage : public classbase
{
public:
	std::string message;
	irc::string target;
	time_t sent;

	BlockedMessage(const std::string &msg, const irc::string &tgt, time_t when)
	: message(msg), target(tgt), sent(when)
	{
	}
};

class ModuleBlockAmsg : public Module
{
	int ForgetDelay;
	BlockAction action;
	
 public:
	ModuleBlockAmsg(InspIRCd* Me)
	: Module(Me)
	{
		
		this->OnRehash(NULL,"");
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
		return Version(1,1,0,0,VF_VENDOR,API_VERSION);
	}
	
	virtual void OnRehash(userrec* user, const std::string &parameter)
	{
		ConfigReader Conf(ServerInstance);
		
		ForgetDelay = Conf.ReadInteger("blockamsg", "delay", 0, false);
		
		if(Conf.GetError() == CONF_VALUE_NOT_FOUND)
			ForgetDelay = -1;
			
		std::string act = Conf.ReadValue("blockamsg", "action", 0);
		
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
	}

	virtual int OnPreCommand(const std::string &command, const char** parameters, int pcnt, userrec *user, bool validated, const std::string &original_line)
	{
		// Don't do anything with unregistered users, or remote ones.
		if(!user || (user->registered != REG_ALL) || !IS_LOCAL(user))
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
		
			if(*parameters[0] != '#')
			{
				// Decrement if the first target wasn't a channel.
				targets--;
			}
			
			for(const char* c = parameters[0]; *c; c++)
				if((*c == ',') && *(c+1) && (*(c+1) == '#'))
					targets++;
				
			/* targets should now contain the number of channel targets the msg/notice was pointed at.
			 * If the msg/notice was a PM there should be no channel targets and 'targets' should = 0.
			 * We don't want to block PMs so...
			 */
			if(targets == 0)
			{
				return 0;
			}
					
			userchans = user->chans.size();

			// Check that this message wasn't already sent within a few seconds.
			BlockedMessage* m;
			user->GetExt("amsgblock", m);
			
			// If the message is identical and within the time.
			// We check the target is *not* identical, that'd straying into the realms of flood control. Which isn't what we're doing...
			// OR
			// The number of target channels is equal to the number of channels the sender is on..a little suspicious.
			// Check it's more than 1 too, or else users on one channel would have fun.
			if((m && (m->message == parameters[1]) && (m->target != parameters[0]) && (ForgetDelay != -1) && (m->sent >= ServerInstance->Time()-ForgetDelay)) || ((targets > 1) && (targets == userchans)))
			{
				// Block it...
				if(action == IBLOCK_KILLOPERS || action == IBLOCK_NOTICEOPERS)
					ServerInstance->WriteOpers("*** %s had an /amsg or /ame denied", user->nick);

				if(action == IBLOCK_KILL || action == IBLOCK_KILLOPERS)
					userrec::QuitUser(ServerInstance, user, "Global message (/amsg or /ame) detected");
				else if(action == IBLOCK_NOTICE || action == IBLOCK_NOTICEOPERS)
					user->WriteServ( "NOTICE %s :Global message (/amsg or /ame) detected", user->nick);
									
				return 1;
			}
			
			if(m)
			{
				// If there's already a BlockedMessage allocated, use it.
				m->message = parameters[1];
				m->target = parameters[0];
				m->sent = ServerInstance->Time();
			}
			else
			{
				m = new BlockedMessage(parameters[1], parameters[0], ServerInstance->Time());
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
			BlockedMessage* m;
			user->GetExt("amsgblock", m);
			if(m)
			{
				DELETE(m);
				user->Shrink("amsgblock");
			}
		}
	}
};


MODULE_INIT(ModuleBlockAmsg)
