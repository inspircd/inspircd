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

#include <stdio.h>
#include <string>
#include <vector>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "inspircd.h"

/* $ModDesc: Provides support for the /SILENCE command */


// This typedef holds a silence list. Each user may or may not have a
// silencelist, if a silence list is empty for a user, he/she does not
// have one of these structures associated with their user record.
typedef std::vector<std::string> silencelist;

class cmd_silence : public command_t
{
 public:
	cmd_silence() : command_t("SILENCE", 0, 0)
	{
		this->source = "m_silence.so";
		syntax = "{[+|-]<nick>}";
	}

	void Handle (const char** parameters, int pcnt, userrec *user)
	{
		if (!pcnt)
		{
			// no parameters, show the current silence list.
			// Use Extensible::GetExt to fetch the silence list
			silencelist* sl;
			user->GetExt("silence_list", sl);
			// if the user has a silence list associated with their user record, show it
			if (sl)
			{
				for (silencelist::const_iterator c = sl->begin(); c < sl->end(); c++)
				{
					user->WriteServ("271 %s %s %s!*@*",user->nick, user->nick,c->c_str());
				}
			}
			user->WriteServ("272 %s :End of Silence List",user->nick);
		}
		else if (pcnt > 0)
		{
			// one or more parameters, add or delete entry from the list (only the first parameter is used)
			const char *nick = parameters[0];
			if (nick[0] == '-')
			{
				// removing an item from the list
				nick++;
				// fetch their silence list
				silencelist* sl;
				user->GetExt("silence_list", sl);
				// does it contain any entries and does it exist?
				if (sl)
				{
					if (sl->size())
					{
						for (silencelist::iterator i = sl->begin(); i != sl->end(); i++)
		       			 	{
							// search through for the item
							irc::string listitem = i->c_str();
							irc::string target = nick;
							if (listitem == target)
		       					{
		       						sl->erase(i);
								user->WriteServ("950 %s %s :Removed %s!*@* from silence list",user->nick, user->nick,nick);
								// we have modified the vector from within a loop, we must now bail out
							       	return;
	       						}
	       					}
					}
					if (!sl->size())
					{
						// tidy up -- if a user's list is empty, theres no use having it
						// hanging around in the user record.
						DELETE(sl);
						user->Shrink("silence_list");
					}
				}
			}
			else if (nick[0] == '+')
			{
				nick++;
				// fetch the user's current silence list
				silencelist* sl;
				user->GetExt("silence_list", sl);
				// what, they dont have one??? WE'RE ALL GONNA DIE! ...no, we just create an empty one.
				if (!sl)
				{
					sl = new silencelist;
					user->Extend(std::string("silence_list"), sl);
				}
				// add the nick to it -- silence only takes nicks for some reason even though its list shows masks
				for (silencelist::iterator n = sl->begin(); n != sl->end();  n++)
				{
					irc::string listitem = n->c_str();
					irc::string target = nick;
					if (listitem == target)
					{
						user->WriteServ("952 %s %s :%s is already on your silence list",user->nick, user->nick,nick);
						return;
					}
				}
				sl->push_back(std::string(nick));
				user->WriteServ("951 %s %s :Added %s!*@* to silence list",user->nick, user->nick,nick);
				return;
			}
		}
		return;
	}
};

class ModuleSilence : public Module
{
	
	cmd_silence* mycommand;
 public:
 
	ModuleSilence(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		mycommand = new cmd_silence();
		ServerInstance->AddCommand(mycommand);
	}

	void Implements(char* List)
	{
		List[I_OnUserQuit] = List[I_On005Numeric] = List[I_OnUserPreNotice] = List[I_OnUserPreMessage] = 1;
	}

	virtual void OnUserQuit(userrec* user, const std::string &reason)
	{
		// when the user quits tidy up any silence list they might have just to keep things tidy
		// and to prevent a HONKING BIG MEMORY LEAK!
		silencelist* sl;
		user->GetExt("silence_list", sl);
		if (sl)
		{
			DELETE(sl);
			user->Shrink("silence_list");
		}
	}

	virtual void On005Numeric(std::string &output)
	{
		// we don't really have a limit...
		output = output + " SILENCE=999";
	}
	
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		// im not sure how unreal's silence operates but ours is sensible. It blocks notices and
		// privmsgs from people on the silence list, directed privately at the user.
		// channel messages are unaffected (ever tried to follow the flow of conversation in
		// a channel when you've set an ignore on the two most talkative people?)
		if (target_type == TYPE_USER)
		{
			userrec* u = (userrec*)dest;
			silencelist* sl;
			u->GetExt("silence_list", sl);
			if (sl)
			{
				for (silencelist::const_iterator c = sl->begin(); c != sl->end(); c++)
				{
					irc::string listitem = c->c_str();
					irc::string target = user->nick;
					if (listitem == target)
					{
						return 1;
					}
				}
			}
		}
		return 0;
	}

	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		return OnUserPreNotice(user,dest,target_type,text,status);
	}

	virtual ~ModuleSilence()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}
};


class ModuleSilenceFactory : public ModuleFactory
{
 public:
	ModuleSilenceFactory()
	{
	}
	
	~ModuleSilenceFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleSilence(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSilenceFactory;
}

