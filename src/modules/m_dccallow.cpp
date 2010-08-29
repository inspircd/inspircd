/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Povides support for the /DCCALLOW command */

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

	DCCAllow(const std::string &nick, const std::string &hm, const time_t so, const long ln) : nickname(nick), hostmask(hm), set_on(so), length(ln) { }
};

typedef std::vector<User *> userlist;
userlist ul;
typedef std::vector<DCCAllow> dccallowlist;
dccallowlist* dl;
typedef std::vector<BannedFileList> bannedfilelist;
bannedfilelist bfl;
SimpleExtItem<dccallowlist>* ext = NULL;

class CommandDccallow : public Command
{
 public:
	CommandDccallow(Module* parent) : Command(parent, "DCCALLOW", 0)
	{
		syntax = "{[+|-]<nick> <time>|HELP|LIST}";
		/* XXX we need to fix this so it can work with translation stuff (i.e. move +- into a seperate param */
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		/* syntax: DCCALLOW [+|-]<nick> (<time>) */
		if (!parameters.size())
		{
			// display current DCCALLOW list
			DisplayDCCAllowList(user);
			return CMD_FAILURE;
		}
		else if (parameters.size() > 0)
		{
			char action = *parameters[0].c_str();

			// if they didn't specify an action, this is probably a command
			if (action != '+' && action != '-')
			{
				if (!strcasecmp(parameters[0].c_str(), "LIST"))
				{
					// list current DCCALLOW list
					DisplayDCCAllowList(user);
					return CMD_FAILURE;
				}
				else if (!strcasecmp(parameters[0].c_str(), "HELP"))
				{
					// display help
					DisplayHelp(user);
					return CMD_FAILURE;
				}
				else
				{
					user->WriteNumeric(998, "%s :DCCALLOW command not understood. For help on DCCALLOW, type /DCCALLOW HELP", user->nick.c_str());
					return CMD_FAILURE;
				}
			}

			std::string nick = parameters[0].substr(1);
			User *target = ServerInstance->FindNickOnly(nick);

			if (target)
			{

				if (action == '-')
				{
					// check if it contains any entries
					dl = ext->get(user);
					if (dl)
					{
						for (dccallowlist::iterator i = dl->begin(); i != dl->end(); ++i)
						{
							// search through list
							if (i->nickname == target->nick)
							{
								dl->erase(i);
								user->WriteNumeric(995, "%s %s :Removed %s from your DCCALLOW list", user->nick.c_str(), user->nick.c_str(), target->nick.c_str());
								break;
							}
						}
					}
				}
				else if (action == '+')
				{
					if (target == user)
					{
						user->WriteNumeric(996, "%s %s :You cannot add yourself to your own DCCALLOW list!", user->nick.c_str(), user->nick.c_str());
						return CMD_FAILURE;
					}

					dl = ext->get(user);
					if (!dl)
					{
						dl = new dccallowlist;
						ext->set(user, dl);
						// add this user to the userlist
						ul.push_back(user);
					}

					for (dccallowlist::const_iterator k = dl->begin(); k != dl->end(); ++k)
					{
						if (k->nickname == target->nick)
						{
							user->WriteNumeric(996, "%s %s :%s is already on your DCCALLOW list", user->nick.c_str(), user->nick.c_str(), target->nick.c_str());
							return CMD_FAILURE;
						}
					}

					std::string mask = std::string(target->nick)+"!"+std::string(target->ident)+"@"+std::string(target->dhost);
					std::string default_length = ServerInstance->Config->GetTag("dccallow")->getString("length");

					long length;
					if (parameters.size() < 2)
					{
						length = ServerInstance->Duration(default_length);
					}
					else if (!atoi(parameters[1].c_str()))
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
						user->WriteNumeric(993, "%s %s :Added %s to DCCALLOW list for %ld seconds", user->nick.c_str(), user->nick.c_str(), target->nick.c_str(), length);
					}
					else
					{
						user->WriteNumeric(994, "%s %s :Added %s to DCCALLOW list for this session", user->nick.c_str(), user->nick.c_str(), target->nick.c_str());
					}

					/* route it. */
					return CMD_SUCCESS;
				}
			}
			else
			{
				// nick doesn't exist
				user->WriteNumeric(401, "%s %s :No such nick/channel", user->nick.c_str(), nick.c_str());
				return CMD_FAILURE;
			}
		}
		return CMD_FAILURE;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_BROADCAST;
	}

	void DisplayHelp(User* user)
	{
		user->WriteNumeric(998, "%s :DCCALLOW [<+|->nick [time]] [list] [help]", user->nick.c_str());
		user->WriteNumeric(998, "%s :You may allow DCCs from specific users by specifying a", user->nick.c_str());
		user->WriteNumeric(998, "%s :DCC allow for the user you want to receive DCCs from.", user->nick.c_str());
		user->WriteNumeric(998, "%s :For example, to allow the user Brain to send you inspircd.exe", user->nick.c_str());
		user->WriteNumeric(998, "%s :you would type:", user->nick.c_str());
		user->WriteNumeric(998, "%s :/DCCALLOW +Brain", user->nick.c_str());
		user->WriteNumeric(998, "%s :Brain would then be able to send you files. They would have to", user->nick.c_str());
		user->WriteNumeric(998, "%s :resend the file again if the server gave them an error message", user->nick.c_str());
		user->WriteNumeric(998, "%s :before you added them to your DCCALLOW list.", user->nick.c_str());
		user->WriteNumeric(998, "%s :DCCALLOW entries will be temporary by default, if you want to add", user->nick.c_str());
		user->WriteNumeric(998, "%s :them to your DCCALLOW list until you leave IRC, type:", user->nick.c_str());
		user->WriteNumeric(998, "%s :/DCCALLOW +Brain 0", user->nick.c_str());
		user->WriteNumeric(998, "%s :To remove the user from your DCCALLOW list, type:", user->nick.c_str());
		user->WriteNumeric(998, "%s :/DCCALLOW -Brain", user->nick.c_str());
		user->WriteNumeric(998, "%s :To see the users in your DCCALLOW list, type:", user->nick.c_str());
		user->WriteNumeric(998, "%s :/DCCALLOW LIST", user->nick.c_str());
		user->WriteNumeric(998, "%s :NOTE: If the user leaves IRC or changes their nickname", user->nick.c_str());
		user->WriteNumeric(998, "%s :  they will be removed from your DCCALLOW list.", user->nick.c_str());
		user->WriteNumeric(998, "%s :  your DCCALLOW list will be deleted when you leave IRC.", user->nick.c_str());
		user->WriteNumeric(999, "%s :End of DCCALLOW HELP", user->nick.c_str());
	}

	void DisplayDCCAllowList(User* user)
	{
		 // display current DCCALLOW list
		user->WriteNumeric(990, "%s :Users on your DCCALLOW list:", user->nick.c_str());

		dl = ext->get(user);
		if (dl)
		{
			for (dccallowlist::const_iterator c = dl->begin(); c != dl->end(); ++c)
			{
				user->WriteNumeric(991, "%s %s :%s (%s)", user->nick.c_str(), user->nick.c_str(), c->nickname.c_str(), c->hostmask.c_str());
			}
		}

		user->WriteNumeric(992, "%s :End of DCCALLOW list", user->nick.c_str());
	}

};

class ModuleDCCAllow : public Module
{
	CommandDccallow cmd;
 public:

	ModuleDCCAllow()
		: cmd(this)
	{
	}

	void init()
	{
		ext = new SimpleExtItem<dccallowlist>("dccallow", this);
		ServerInstance->Extensions.Register(ext);
		ServerInstance->AddCommand(&cmd);
		ReadFileConf();
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice, I_OnUserQuit, I_OnUserPreNick };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}


	void ReadConfig(ConfigReadStatus&)
	{
		ReadFileConf();
	}

	virtual void OnUserQuit(User* user, const std::string &reason, const std::string &oper_message)
	{
		dccallowlist* udl = ext->get(user);

		// remove their DCCALLOW list if they have one
		if (udl)
		{
			RemoveFromUserlist(user);
		}

		// remove them from any DCCALLOW lists
		// they are currently on
		RemoveNick(user);
	}

	virtual ModResult OnUserPreNick(User* user, const std::string &newnick)
	{
		RemoveNick(user);
		return MOD_RES_PASSTHRU;
	}

	virtual ModResult OnUserPreMessage(User* user, void* dest, int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreNotice(user, dest, target_type, text, status, exempt_list);
	}

	virtual ModResult OnUserPreNotice(User* user, void* dest, int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (!IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		if (target_type == TYPE_USER)
		{
			User* u = (User*)dest;

			/* Always allow a user to dcc themselves (although... why?) */
			if (user == u)
				return MOD_RES_PASSTHRU;

			if ((text.length()) && (text[0] == '\1'))
			{
				Expire();

				// :jamie!jamie@test-D4457903BA652E0F.silverdream.org PRIVMSG eimaj :DCC SEND m_dnsbl.cpp 3232235786 52650 9676
				// :jamie!jamie@test-D4457903BA652E0F.silverdream.org PRIVMSG eimaj :VERSION

				if (strncmp(text.c_str(), "\1DCC ", 5) == 0)
				{
					dl = ext->get(u);
					if (dl && dl->size())
					{
						for (dccallowlist::const_iterator iter = dl->begin(); iter != dl->end(); ++iter)
							if (InspIRCd::Match(user->GetFullHost(), iter->hostmask))
								return MOD_RES_PASSTHRU;
					}

					// tokenize
					std::stringstream ss(text);
					std::string buf;
					std::vector<std::string> tokens;

					while (ss >> buf)
						tokens.push_back(buf);

					irc::string type = tokens[1].c_str();

					bool blockchat = ServerInstance->Config->GetTag("dccallow")->getBool("blockchat");

					if (type == "SEND")
					{
						std::string defaultaction = ServerInstance->Config->GetTag("dccallow")->getString("action");
						std::string filename = tokens[2];

						bool found = false;
						for (unsigned int i = 0; i < bfl.size(); i++)
						{
							if (InspIRCd::Match(filename, bfl[i].filemask, ascii_case_insensitive_map))
							{
								/* We have a matching badfile entry, override whatever the default action is */
								if (bfl[i].action == "allow")
									return MOD_RES_PASSTHRU;
								else
								{
									found = true;
									break;
								}
							}
						}

						/* only follow the default action if no badfile matches were found above */
						if ((!found) && (defaultaction == "allow"))
							return MOD_RES_PASSTHRU;

						user->WriteServ("NOTICE %s :The user %s is not accepting DCC SENDs from you. Your file %s was not sent.", user->nick.c_str(), u->nick.c_str(), filename.c_str());
						u->WriteServ("NOTICE %s :%s (%s@%s) attempted to send you a file named %s, which was blocked.", u->nick.c_str(), user->nick.c_str(), user->ident.c_str(), user->dhost.c_str(), filename.c_str());
						u->WriteServ("NOTICE %s :If you trust %s and were expecting this, you can type /DCCALLOW HELP for information on the DCCALLOW system.", u->nick.c_str(), user->nick.c_str());
						return MOD_RES_DENY;
					}
					else if ((type == "CHAT") && (blockchat))
					{
						user->WriteServ("NOTICE %s :The user %s is not accepting DCC CHAT requests from you.", user->nick.c_str(), u->nick.c_str());
						u->WriteServ("NOTICE %s :%s (%s@%s) attempted to initiate a DCC CHAT session, which was blocked.", u->nick.c_str(), user->nick.c_str(), user->ident.c_str(), user->dhost.c_str());
						u->WriteServ("NOTICE %s :If you trust %s and were expecting this, you can type /DCCALLOW HELP for information on the DCCALLOW system.", u->nick.c_str(), user->nick.c_str());
						return MOD_RES_DENY;
					}
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}

	void Expire()
	{
		for (userlist::iterator iter = ul.begin(); iter != ul.end(); ++iter)
		{
			User* u = (User*)(*iter);
			dl = ext->get(u);
			if (dl)
			{
				if (dl->size())
				{
					dccallowlist::iterator iter2 = dl->begin();
					while (iter2 != dl->end())
					{
						if (iter2->length != 0 && (iter2->set_on + iter2->length) <= ServerInstance->Time())
						{
							u->WriteNumeric(997, "%s %s :DCCALLOW entry for %s has expired", u->nick.c_str(), u->nick.c_str(), iter2->nickname.c_str());
							iter2 = dl->erase(iter2);
						}
						else
						{
							++iter2;
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

	void RemoveNick(User* user)
	{
		/* Iterate through all DCCALLOW lists and remove user */
		for (userlist::iterator iter = ul.begin(); iter != ul.end(); ++iter)
		{
			User *u = (User*)(*iter);
			dl = ext->get(u);
			if (dl)
			{
				if (dl->size())
				{
					for (dccallowlist::iterator i = dl->begin(); i != dl->end(); ++i)
					{
						if (i->nickname == user->nick)
						{

							u->WriteServ("NOTICE %s :%s left the network or changed their nickname and has been removed from your DCCALLOW list", u->nick.c_str(), i->nickname.c_str());
							u->WriteNumeric(995, "%s %s :Removed %s from your DCCALLOW list", u->nick.c_str(), u->nick.c_str(), i->nickname.c_str());
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

	void RemoveFromUserlist(User *user)
	{
		// remove user from userlist
		for (userlist::iterator j = ul.begin(); j != ul.end(); ++j)
		{
			User* u = (User*)(*j);
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
		ConfigTagList tags = ServerInstance->Config->GetTags("banfile");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			BannedFileList bf;
			bf.filemask = i->second->getString("pattern");
			bf.action = i->second->getString("action");
			bfl.push_back(bf);
		}
	}

	virtual ~ModuleDCCAllow()
	{
		delete ext;
	}

	virtual Version GetVersion()
	{
		return Version("Povides support for the /DCCALLOW command", VF_COMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleDCCAllow)
