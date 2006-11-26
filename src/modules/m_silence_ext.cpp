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
#include <stdarg.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "hashcomp.h"
#include "inspircd.h"
#include "wildcard.h"

/* $ModDesc: Provides support for the /SILENCE command */

/* Improved drop-in replacement for the /SILENCE command
 * syntax: /SILENCE [+|-]<mask> <p|c|i|n|a|x> as in <private|channel|invites|notices|all|exclude>
 *
 * example that blocks all except private messages
 *  /SILENCE +*!*@* a
 *  /SILENCE +*!*@* px
 *
 * example that blocks all invites except from channel services
 *  /SILENCE +*!*@* i
 *  /SILENCE +chanserv!services@chatters.net ix
 *
 * example that blocks some bad dude from private, notice and inviting you
 *  /SILENCE +*!kiddie@lamerz.net pin
 *
 * TODO: possibly have add and remove check for existing host and only modify flags according to
 *       what's been changed instead of having to remove first, then add if you want to change
 *       an entry.
 */

// pair of hostmask and flags
typedef std::pair<std::string, int> silenceset;

// deque list of pairs
typedef std::deque<silenceset> silencelist;

// intmasks for flags
static int SILENCE_PRIVATE	= 0x0001; /* p  private messages      */
static int SILENCE_CHANNEL	= 0x0002; /* c  channel messages      */
static int SILENCE_INVITE	= 0x0004; /* i  invites               */
static int SILENCE_NOTICE	= 0x0008; /* n  notices               */
static int SILENCE_ALL		= 0x0010; /* a  all, (pcin)           */
static int SILENCE_EXCLUDE	= 0x0020; /* x  exclude this pattern  */


class cmd_silence : public command_t
{
 public:
	cmd_silence (InspIRCd* Instance) : command_t(Instance,"SILENCE", 0, 0)
	{
		this->source = "m_silence_ext.so";
		syntax = "{[+|-]<mask> <p|c|i|n|a|x>}";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
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
				for (silencelist::const_iterator c = sl->begin(); c != sl->end(); c++)
				{
					user->WriteServ("271 %s %s %s %s",user->nick, user->nick,c->first.c_str(), DecompPattern(c->second).c_str());
				}
			}
			user->WriteServ("272 %s :End of Silence List",user->nick);

			return CMD_SUCCESS;
		}
		else if (pcnt > 0)
		{
			// one or more parameters, add or delete entry from the list (only the first parameter is used)
			std::string mask = parameters[0] + 1;
			char action = *parameters[0];
			// Default is private and notice so clients do not break
			int pattern = CompilePattern("pn");

			// if pattern supplied, use it
			if (pcnt > 1) {
				pattern = CompilePattern(parameters[1]);
			}
			
			if (!mask.length())
 			{
				// 'SILENCE +' or 'SILENCE -', assume *!*@*
				mask = "*!*@*";
			}
			
			ModeParser::CleanMask(mask);

			if (action == '-')
			{
				// fetch their silence list
				silencelist* sl;
				user->GetExt("silence_list", sl);
				// does it contain any entries and does it exist?
				if (sl)
				{
					if (sl->size())
					{
						silencelist::iterator i,safei;
						for (i = sl->begin(); i != sl->end(); i++)
						{
							// search through for the item
							irc::string listitem = i->first.c_str();
							if (listitem == mask && i->second == pattern)
							{
								safei = i;
								--i;
								sl->erase(safei);
								user->WriteServ("950 %s %s :Removed %s %s from silence list",user->nick, user->nick, mask.c_str(), DecompPattern(pattern).c_str());
								break;
							}
						}
					}
					else
					{
						// tidy up -- if a user's list is empty, theres no use having it
						// hanging around in the user record.
						DELETE(sl);
						user->Shrink("silence_list");
					}
				}
			}
			else if (action == '+')
			{
				// fetch the user's current silence list
				silencelist* sl;
				user->GetExt("silence_list", sl);
				// what, they dont have one??? WE'RE ALL GONNA DIE! ...no, we just create an empty one.
				if (!sl)
				{
					sl = new silencelist;
					user->Extend("silence_list", sl);
				}
				for (silencelist::iterator n = sl->begin(); n != sl->end();  n++)
				{
					irc::string listitem = n->first.c_str();
					if (listitem == mask && n->second == pattern)
					{
						user->WriteServ("952 %s %s :%s %s is already on your silence list",user->nick, user->nick, mask.c_str(), DecompPattern(pattern).c_str());
						return CMD_SUCCESS;
					}
				}
				if (((pattern & SILENCE_EXCLUDE) > 0))
				{
					sl->push_front(silenceset(mask,pattern));
				}
				else
				{
					sl->push_back(silenceset(mask,pattern));
				}
				user->WriteServ("951 %s %s :Added %s %s to silence list",user->nick, user->nick, mask.c_str(), DecompPattern(pattern).c_str());
				return CMD_SUCCESS;
			}
		}
		return CMD_SUCCESS;
	}

	/* turn the nice human readable pattern into a mask */
	int CompilePattern(const char* pattern)
	{
		int p = 0;
		for (uint n = 0; n < strlen(pattern); n++)
		{
			switch (pattern[n])
			{
				case 'p':
					p |= SILENCE_PRIVATE;
					break;
				case 'c':
					p |= SILENCE_CHANNEL;
					break;
				case 'i': 
					p |= SILENCE_INVITE;
					break;
				case 'n':
					p |= SILENCE_NOTICE;
					break;
				case 'a':
					p |= SILENCE_ALL;
					break;
				case 'x':
					p |= SILENCE_EXCLUDE;
					break;
				default:
					break;
			}
		}
		return p;
	}

	/* turn the mask into a nice human readable format */
	std::string DecompPattern (const int pattern)
	{
		std::string out = "";
		if ((pattern & SILENCE_PRIVATE) > 0)
			out += ",private";
		if ((pattern & SILENCE_CHANNEL) > 0)
			out += ",channel";
		if ((pattern & SILENCE_INVITE) > 0)
			out += ",invites";
		if ((pattern & SILENCE_NOTICE) > 0)
			out += ",notices";
		if ((pattern & SILENCE_ALL) > 0)
			out = ",all";
		if ((pattern & SILENCE_EXCLUDE) > 0)
			out += ",exclude";
		return "<" + out.substr(1) + ">";
	}

};

class ModuleSilence : public Module
{
	
	cmd_silence* mycommand;
 public:
 
	ModuleSilence(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		mycommand = new cmd_silence(ServerInstance);
		ServerInstance->AddCommand(mycommand);
	}

	void Implements(char* List)
	{
		List[I_OnUserQuit] = List[I_On005Numeric] = List[I_OnUserPreNotice] = List[I_OnUserPreMessage] = 1;
		List[I_OnUserPreInvite] = 1;
		List[I_OnPreCommand] = 1;
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

	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		if (target_type == TYPE_USER)
		{
			return MatchPattern((userrec*)dest, user, SILENCE_PRIVATE);
		}
		return 0;
	}

	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		return MatchPattern((userrec*)dest, user, SILENCE_NOTICE);
	}

	virtual int OnUserPreInvite(userrec* source,userrec* dest,chanrec* channel)
	{
		return MatchPattern(dest, source, SILENCE_INVITE);
	}

	int MatchPattern(userrec* dest, userrec* source, int pattern)
	{
		silencelist* sl;
		dest->GetExt("silence_list", sl);
		if (sl)
		{
			for (silencelist::const_iterator c = sl->begin(); c != sl->end(); c++)
			{
				if ((match(source->GetFullHost(), c->first.c_str())) && ( ((c->second & pattern) > 0)) || ((c->second & SILENCE_ALL) > 0))
				{
					if (((c->second & SILENCE_EXCLUDE) > 0))
					{
						return 0;
					}
					else {
						return 1;
					}
				}
			}
		}
		return 0;
	}

	virtual int OnPreCommand(const std::string &command, const char** parameters, int pcnt, userrec *user, bool validated, const std::string &original_line)
	{
		/* Implement the part of cmd_privmsg.cpp that handles *channel* messages, if cmd_privmsg.cpp
		 * is changed this probably needs updating too. Also implement the actual write to the users
		 * on the channel. This code is from channels.cpp, and should also be changed if channels.cpp
		 * updates it's corresponding code
		 */
		if ((validated) && (command == "PRIVMSG"))
		{
			char status = 0;
			if ((*parameters[0] == '@') || (*parameters[0] == '%') || (*parameters[0] == '+'))
			{
				status = *parameters[0];
				parameters[0]++;
			}
			if (parameters[0][0] == '#')
			{
				chanrec *chan;
				user->idle_lastmsg = ServerInstance->Time();
				chan = ServerInstance->FindChan(parameters[0]);
				if (chan)
				{
					if (IS_LOCAL(user))
					{
						if ((chan->modes[CM_NOEXTERNAL]) && (!chan->HasUser(user)))
						{
							user->WriteServ("404 %s %s :Cannot send to channel (no external messages)", user->nick, chan->name);
							return 1;
						}
						if ((chan->modes[CM_MODERATED]) && (chan->GetStatus(user) < STATUS_VOICE))
						{
							user->WriteServ("404 %s %s :Cannot send to channel (+m)", user->nick, chan->name);
							return 1;
						}
					}
					int MOD_RESULT = 0;

					std::string temp = parameters[1];
					FOREACH_RESULT(I_OnUserPreMessage,OnUserPreMessage(user,chan,TYPE_CHANNEL,temp,status));
					if (MOD_RESULT) {
						return 1;
					}
					parameters[1] = temp.c_str();

					if (temp == "")
					{
						user->WriteServ("412 %s No text to send", user->nick);
						return 1;
					}

					/* This next call into channel.cpp is the one that gets replaced by our modified method
					 * chan->WriteAllExceptSender(user, false, status, "PRIVMSG %s :%s", chan->name, parameters[1]);
					 */
					WriteAllExceptSenderAndSilenced(chan, user, false, status, "PRIVMSG %s :%s", chan->name, parameters[1]);

					FOREACH_MOD(I_OnUserMessage,OnUserMessage(user,chan,TYPE_CHANNEL,parameters[1],status));
					return 1;
				}
				else
				{
					/* no such nick/channel */
					user->WriteServ("401 %s %s :No such nick/channel",user->nick, parameters[0]);
					return 1;
				}
				return 1;
			}
			else
			{
				command_t* privmsg_command = ServerInstance->Parser->GetHandler("PRIVMSG");
				if (privmsg_command)
				{
					privmsg_command->Handle(parameters, pcnt, user);
					return 1;
				}
				else
				{
					ServerInstance->Log(DEBUG, "Could not find PRIVMSG Command!");
				}
			}
		}
		return 0;
	}

	/* Taken from channels.cpp and slightly modified, see OnPreCommand above*/
	void WriteAllExceptSenderAndSilenced(chanrec* chan, userrec* user, bool serversource, char status, char* text, ...)
	{
		char textbuffer[MAXBUF];
		va_list argsPtr;

		if (!text)
			return;

		va_start(argsPtr, text);
		vsnprintf(textbuffer, MAXBUF, text, argsPtr);
		va_end(argsPtr);

		this->WriteAllExceptSenderAndSilenced(chan, user, serversource, status, std::string(textbuffer));
	}

	/* Taken from channels.cpp and slightly modified, see OnPreCommand above*/
	void WriteAllExceptSenderAndSilenced(chanrec* chan, userrec* user, bool serversource, char status, const std::string& text)
	{
		CUList *ulist;

		switch (status)
		{
			case '@':
				ulist = chan->GetOppedUsers();
				break;
			case '%':
				ulist = chan->GetHalfoppedUsers();
				break;
			case '+':
				ulist = chan->GetVoicedUsers();
				break;
			default:
				ulist = chan->GetUsers();
				break;
		}
	
		for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
		{
			if ((IS_LOCAL(i->second)) && (user != i->second))
			{
				if (serversource)
				{
					i->second->WriteServ(text);
				}
				else
				{
					if (MatchPattern(i->second, user, SILENCE_CHANNEL) == 0)
					{
						i->second->WriteFrom(user,text);
					}
				}
			}
		}
	}

	virtual ~ModuleSilence()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
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

