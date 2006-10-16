 /* m_dccallow - Jamie Penman-Smithson <jamie@silverdream.org> - September 2006 */

using namespace std;

#include <stdio.h>
#include <vector>
#include <string.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"

/* $ModDesc: Povides support for the /DCCALLOW command */

static ConfigReader *Conf;

class BannedFileList
{
 public:
	std::string filemask;
	std::string action;
};

class DCCAllow
{
 public:
	std::string nickname;
	std::string hostmask;
	time_t set_on;
	long length;

	DCCAllow() { }

	DCCAllow(std::string nick, std::string hm, time_t so, long ln) : nickname(nick), hostmask(hm), set_on(so), length(ln) { }
};

typedef std::vector<userrec *> userlist;
userlist ul;
typedef std::vector<DCCAllow> dccallowlist;
dccallowlist* dl;
typedef std::vector<BannedFileList> bannedfilelist;
bannedfilelist bfl;

class cmd_dccallow : public command_t
{
 public:
	cmd_dccallow(InspIRCd* Me) : command_t(Me, "DCCALLOW", 0, 0)
	{
		this->source = "m_dccallow.so";
		syntax = "{[+|-]<nick> <time>}";
	}

	CmdResult Handle(const char **parameters, int pcnt, userrec *user)
	{
		/* syntax: DCCALLOW [+|-]<nick> (<time>) */
		if (!pcnt)
		{
			// display current DCCALLOW list
			DisplayDCCAllowList(user);
			return CMD_SUCCESS;
		}
		else if (pcnt > 0)
		{
			char action = *parameters[0];
		
			// if they didn't specify an action, this is probably a command
			if (action != '+' && action != '-')
			{
				if (!strcasecmp(parameters[0], "LIST"))
				{
					// list current DCCALLOW list
					DisplayDCCAllowList(user);
					return CMD_SUCCESS;
				} 
				else if (!strcasecmp(parameters[0], "HELP"))
				{
					// display help
					DisplayHelp(user);
					return CMD_SUCCESS;
				}
			}
			
			std::string nick = parameters[0] + 1;
			userrec *target = ServerInstance->FindNick(nick);
	
			if (target)
			{
				ServerInstance->Log(DEBUG, "m_dccallow.so: got target %s and action %c", target->nick, action);
				
				if (action == '-')
				{
					user->GetExt("dccallow_list", dl);
					// check if it contains any entries
					if (dl)
					{
						if (dl->size())
						{
							for (dccallowlist::iterator i = dl->begin(); i != dl->end(); ++i)
							{
								// search through list
								if (i->nickname == target->nick)
								{
									dl->erase(i);
									user->WriteServ("995 %s %s :Removed %s from your DCCALLOW list", user->nick, user->nick, target->nick);
									break;
								}
							}
						}
					}
					else
					{
						DELETE(dl);
						user->Shrink("dccallow_list");
				
						// remove from userlist
						for (userlist::iterator j = ul.begin(); j != ul.end(); ++j)
						{
							userrec* u = (userrec*)(*j);
							if (u == user)
							{
								ul.erase(j);
								break;
							}
						}
					}
				}
				else if (action == '+')
				{
					// fetch current DCCALLOW list
					user->GetExt("dccallow_list", dl);
					// they don't have one, create it
					if (!dl)
					{
						dl = new dccallowlist;
						user->Extend(std::string("dccallow_list"), dl);
						// add this user to the userlist
						ul.push_back(user);
					}
					for (dccallowlist::const_iterator k = dl->begin(); k != dl->end(); ++k)
					{
						if (k->nickname == target->nick)
						{
							user->WriteServ("996 %s %s :%s is already on your DCCALLOW list", user->nick, user->nick, target->nick);
							return CMD_SUCCESS;
						}
					}
				
					std::string mask = std::string(target->nick)+"!"+std::string(target->ident)+"@"+std::string(target->dhost);
					std::string default_length = Conf->ReadValue("dccallow", "length", 0).c_str();
		
					long length;
					if (pcnt == 1 || ServerInstance->Duration(parameters[1]) < 1)
					{
						length = ServerInstance->Duration(default_length.c_str());
					} 
					else if (parameters[1] == 0)
					{
						length = 0;
					}
					else
					{
						length = ServerInstance->Duration(parameters[1]);
					}
	
					if (!ServerInstance->IsValidMask(mask.c_str()))
					{
						return CMD_FAILURE;
					}
			
					dl->push_back(DCCAllow(target->nick, mask, ServerInstance->Time(), length));
			
					if (length > 0)
					{
						user->WriteServ("993 %s %s :Added %s to DCCALLOW list for %d seconds", user->nick, user->nick, target->nick, length);
					}
					else
					{
						user->WriteServ("994 %s %s :Added %s to DCCALLOW list for this session", user->nick, user->nick, target->nick);
					}
				
					return CMD_SUCCESS;
				}
			}
			else
			{
				// nick doesn't exist
				user->WriteServ("401 %s %s :No such nick/channel", user->nick, nick.c_str());
				return CMD_FAILURE;
			}
		}
		return CMD_SUCCESS;
	}

	void DisplayHelp(userrec* user)
	{
		user->WriteServ("998 %s :DCCALLOW [<+|->nick [time]] [list] [help]", user->nick);
		user->WriteServ("998 %s :You may allow DCCs from specific users by specifying a", user->nick);
		user->WriteServ("998 %s :DCC allow for the user you want to receive DCCs from.", user->nick);
		user->WriteServ("998 %s :For example, to allow the user Brain to send you inspircd.exe", user->nick);
		user->WriteServ("998 %s :you would type:", user->nick);
		user->WriteServ("998 %s :/DCCALLOW +Brain", user->nick);
		user->WriteServ("998 %s :Brain would then be able to send you files. They would have to", user->nick);
		user->WriteServ("998 %s :resend the file again if the server gave them an error message", user->nick);
		user->WriteServ("998 %s :before you added them to your DCCALLOW list.", user->nick);
		user->WriteServ("998 %s :DCCALLOW entries will be temporary by default, if you want to add", user->nick);
		user->WriteServ("998 %s :them to your DCCALLOW list until you leave IRC, type:", user->nick);
		user->WriteServ("998 %s :/DCCALLOW +Brain 0", user->nick);
		user->WriteServ("998 %s :To remove the user from your DCCALLOW list, type:", user->nick);
		user->WriteServ("998 %s :/DCCALLOW -Brain", user->nick);
		user->WriteServ("998 %s :To see the users in your DCCALLOW list, type:", user->nick);
		user->WriteServ("998 %s :/DCCALLOW LIST", user->nick);
		user->WriteServ("998 %s :NOTE: If the user leaves IRC or changes their nickname", user->nick);
		user->WriteServ("998 %s :  they will be removed from your DCCALLOW list.", user->nick);
		user->WriteServ("998 %s :  your DCCALLOW list will be deleted when you leave IRC.", user->nick);
		user->WriteServ("999 %s :End of DCCALLOW HELP", user->nick);
	}
	
	void DisplayDCCAllowList(userrec* user)
	{
		 // display current DCCALLOW list
		user->WriteServ("990 %s :Users on your DCCALLOW list:", user->nick);
		user->GetExt("dccallow_list", dl);
		
		if (dl)
		{
			for (dccallowlist::const_iterator c = dl->begin(); c != dl->end(); ++c)
			{
				user->WriteServ("991 %s %s :%s (%s)", user->nick, user->nick, c->nickname.c_str(), c->hostmask.c_str());
			}
		}
		
		user->WriteServ("992 %s :End of DCCALLOW list", user->nick);
	}			

};
	
class ModuleDCCAllow : public Module
{
	cmd_dccallow* mycommand;
 public:

	ModuleDCCAllow(InspIRCd* Me)
		: Module::Module(Me)
	{
		Conf = new ConfigReader(ServerInstance);
		mycommand = new cmd_dccallow(ServerInstance);
		ServerInstance->AddCommand(mycommand);
		ReadFileConf();
	}

	void Implements(char* List)
	{
		List[I_OnUserPreMessage] = List[I_OnUserPreNotice] = List[I_OnUserQuit] = List[I_OnUserPreNick] = List[I_OnRehash] = 1;
	}

	virtual void OnRehash(const std::string &parameter)
	{
		delete Conf;
		Conf = new ConfigReader(ServerInstance);
	}

	virtual void OnUserQuit(userrec* user, const std::string &reason)
	{
		dccallowlist* dl;
	
		// remove their DCCALLOW list if they have one
		user->GetExt("dccallow_list", dl);
		if (dl)
		{
			DELETE(dl);
			user->Shrink("dccallow_list");
			RemoveFromUserlist(user);
		}
		
		// remove them from any DCCALLOW lists
		// they are currently on
		RemoveNick(user);
	}


	virtual int OnUserPreNick(userrec* user, const std::string &newnick)
	{
		RemoveNick(user);
		return 0;
	}

	virtual int OnUserPreMessage(userrec* user, void* dest, int target_type, std::string &text, char status)
	{
		return OnUserPreNotice(user, dest, target_type, text, status);
	}

	virtual int OnUserPreNotice(userrec* user, void* dest, int target_type, std::string &text, char status)
	{
		Expire();
	
		if (target_type == TYPE_USER)
		{
			userrec* u = (userrec*)dest;
		
			if ((text.length()) && (text[0] == '\1'))
			{
				// :jamie!jamie@test-D4457903BA652E0F.silverdream.org PRIVMSG eimaj :DCC SEND m_dnsbl.cpp 3232235786 52650 9676
				// :jamie!jamie@test-D4457903BA652E0F.silverdream.org PRIVMSG eimaj :VERSION
					
				if (strncmp(text.c_str(), "\1DCC ", 5) == 0)
				{
					u->GetExt("dccallow_list", dl);
		
					if (dl)
					{
						if (dl->size())
						{
							for (dccallowlist::const_iterator iter = dl->begin(); iter != dl->end(); ++iter)
							{
								if (ServerInstance->MatchText(user->GetFullHost(), iter->hostmask))
								{
									return 0;
								}
							}
						}
					}
		
					// tokenize
					stringstream ss(text);
					std::string buf;
					vector<string> tokens;
		
					while (ss >> buf)
						tokens.push_back(buf);
		
					irc::string type = tokens[1].c_str();
					ServerInstance->Log(DEBUG, "m_dccallow.so: got DCC type %s", type.c_str());
		
					bool blockchat = Conf->ReadFlag("dccallow", "blockchat", 0);
		
					if (type == "SEND")
					{
						std::string defaultaction = Conf->ReadValue("dccallow", "action", 0);
						std::string filename = tokens[2];
					
						if (defaultaction == "allow") 
						{
							return 0;
						}
				
						for (unsigned int i = 0; i < bfl.size(); i++)
						{
							if (ServerInstance->MatchText(filename, bfl[i].filemask))
							{
								if (strcmp(bfl[i].action.c_str(), "allow") == 0)
								{
									return 0;
								}
							}
							else
							{
								if (defaultaction == "allow")
								{
									return 0;
								}
							}
							user->WriteServ("NOTICE %s :The user %s is not accepting DCC SENDs from you. Your file %s was not sent.", user->nick, u->nick, filename.c_str());
							u->WriteServ("NOTICE %s :%s (%s@%s) attempted to send you a file named %s, which was blocked.", u->nick, user->nick, user->ident, user->dhost, filename.c_str());
							u->WriteServ("NOTICE %s :If you trust %s and were expecting this, you can type /DCCALLOW HELP for information on the DCCALLOW system.", u->nick, user->nick);
						}
					}
					else if ((type == "CHAT") && (blockchat))
					{
						user->WriteServ("NOTICE %s :The user %s is not accepting DCC CHAT requests from you.", user->nick, u->nick);
						u->WriteServ("NOTICE %s :%s (%s@%s) attempted to initiate a DCC CHAT session, which was blocked.", u->nick, user->nick, user->ident, user->dhost);
						u->WriteServ("NOTICE %s :If you trust %s and were expecting this, you can type /DCCALLOW HELP for information on the DCCALLOW system.", u->nick, user->nick);
					}
					return 1;
				}
			}
		}
		return 0;
	}
	
	void Expire()
	{
		for (userlist::iterator iter = ul.begin(); iter != ul.end(); ++iter)
		{
			userrec* u = (userrec*)(*iter);
			u->GetExt("dccallow_list", dl);
	
			if (dl)
			{
				if (dl->size())
				{
					dccallowlist::iterator iter = dl->begin();
					while (iter != dl->end())
					{
						if ((iter->set_on + iter->length) <= ServerInstance->Time())
						{
							u->WriteServ("997 %s %s :DCCALLOW entry for %s has expired", u->nick, u->nick, iter->nickname.c_str());
							iter = dl->erase(iter);
						}
						else
						{
							++iter;
						}
					}
				}
			}
			else
			{
				RemoveFromUserlist(u);
				ServerInstance->Log(DEBUG, "m_dccallow.so: UH OH! Couldn't get DCCALLOW list for %s", u->nick);
			}
		}
	}
	
	void RemoveNick(userrec* user)
	{
		/* Iterate through all DCCALLOW lists and remove user */
		for (userlist::iterator iter = ul.begin(); iter != ul.end(); ++iter)
		{
			userrec *u = (userrec*)(*iter);
			u->GetExt("dccallow_list", dl);
	
			if (dl)
			{
				if (dl->size())
				{
					for (dccallowlist::iterator i = dl->begin(); i != dl->end(); ++i)
					{
						if (i->nickname == user->nick)
						{
					
							u->WriteServ("NOTICE %s :%s left the network or changed their nickname and has been removed from your DCCALLOW list", u->nick, i->nickname.c_str());
							u->WriteServ("995 %s %s :Removed %s from your DCCALLOW list", u->nick, u->nick, i->nickname.c_str());
							dl->erase(i);
							break;
						}
					}
				}
			}
			else
			{
				RemoveFromUserlist(u);
			}
		}
	}

	void RemoveFromUserlist(userrec *user)
	{
		// remove user from userlist
		for (userlist::iterator j = ul.begin(); j != ul.end(); ++j)
		{
			userrec* u = (userrec*)(*j);
			if (u == user)
			{
				ul.erase(j);
				break;
			}
		}
	}

	void ReadFileConf()
	{
		bfl.clear();
		for (int i = 0; i < Conf->Enumerate("banfile"); i++)
		{
			BannedFileList bf;
			std::string fileglob = Conf->ReadValue("banfile", "pattern", i);
			std::string action = Conf->ReadValue("banfile", "action", i);
			bf.filemask = fileglob;
			bf.action = action;
			bfl.push_back(bf);
		}
	
	}

	virtual ~ModuleDCCAllow()
	{
	}

	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_COMMON,API_VERSION);
	}
};

class ModuleDCCAllowFactory : public ModuleFactory
{
 public:
	ModuleDCCAllowFactory()
	{
	}

	~ModuleDCCAllowFactory()
	{
	}

	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleDCCAllow(Me);
	}

};

extern "C" void * init_module( void )
{
	return new ModuleDCCAllowFactory;
}
