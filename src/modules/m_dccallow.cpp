/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 John Brooks <john.brooks@dereferenced.net>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2006-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Jamie ??? <???@???>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"

static const char* const helptext[] =
{
	"DCCALLOW [(+|-)<nick> [<time>]]|[LIST|HELP]",
	"You may allow DCCs from specific users by specifying a",
	"DCC allow for the user you want to receive DCCs from.",
	"For example, to allow the user Brain to send you inspircd.exe",
	"you would type:",
	"/DCCALLOW +Brain",
	"Brain would then be able to send you files. They would have to",
	"resend the file again if the server gave them an error message",
	"before you added them to your DCCALLOW list.",
	"DCCALLOW entries will be temporary by default, if you want to add",
	"them to your DCCALLOW list until you leave IRC, type:",
	"/DCCALLOW +Brain 0",
	"To remove the user from your DCCALLOW list, type:",
	"/DCCALLOW -Brain",
	"To see the users in your DCCALLOW list, type:",
	"/DCCALLOW LIST",
	"NOTE: If the user leaves IRC or changes their nickname",
	"  they will be removed from your DCCALLOW list.",
	"  your DCCALLOW list will be deleted when you leave IRC."
};

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
typedef SimpleExtItem<dccallowlist> DCCAllowExt;

class CommandDccallow : public Command
{
	DCCAllowExt& ext;

 public:
	unsigned int maxentries;
	CommandDccallow(Module* parent, DCCAllowExt& Ext)
		: Command(parent, "DCCALLOW", 0)
		, ext(Ext)
	{
		syntax = "[(+|-)<nick> [<time>]]|[LIST|HELP]";
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
					user->WriteNumeric(998, "DCCALLOW command not understood. For help on DCCALLOW, type /DCCALLOW HELP");
					return CMD_FAILURE;
				}
			}

			std::string nick(parameters[0], 1);
			User *target = ServerInstance->FindNickOnly(nick);

			if ((target) && (!target->quitting) && (target->registered == REG_ALL))
			{

				if (action == '-')
				{
					// check if it contains any entries
					dl = ext.get(user);
					if (dl)
					{
						for (dccallowlist::iterator i = dl->begin(); i != dl->end(); ++i)
						{
							// search through list
							if (i->nickname == target->nick)
							{
								dl->erase(i);
								user->WriteNumeric(995, user->nick, InspIRCd::Format("Removed %s from your DCCALLOW list", target->nick.c_str()));
								break;
							}
						}
					}
				}
				else if (action == '+')
				{
					if (target == user)
					{
						user->WriteNumeric(996, user->nick, "You cannot add yourself to your own DCCALLOW list!");
						return CMD_FAILURE;
					}

					dl = ext.get(user);
					if (!dl)
					{
						dl = new dccallowlist;
						ext.set(user, dl);
						// add this user to the userlist
						ul.push_back(user);
					}

					if (dl->size() >= maxentries)
					{
						user->WriteNumeric(996, user->nick, "Too many nicks on DCCALLOW list");
						return CMD_FAILURE;
					}

					for (dccallowlist::const_iterator k = dl->begin(); k != dl->end(); ++k)
					{
						if (k->nickname == target->nick)
						{
							user->WriteNumeric(996, user->nick, InspIRCd::Format("%s is already on your DCCALLOW list", target->nick.c_str()));
							return CMD_FAILURE;
						}
					}

					std::string mask = target->nick+"!"+target->ident+"@"+target->dhost;
					std::string default_length = ServerInstance->Config->ConfValue("dccallow")->getString("length");

					unsigned long length;
					if (parameters.size() < 2)
					{
						length = InspIRCd::Duration(default_length);
					}
					else if (!atoi(parameters[1].c_str()))
					{
						length = 0;
					}
					else
					{
						length = InspIRCd::Duration(parameters[1]);
					}

					if (!InspIRCd::IsValidMask(mask))
					{
						return CMD_FAILURE;
					}

					dl->push_back(DCCAllow(target->nick, mask, ServerInstance->Time(), length));

					if (length > 0)
					{
						user->WriteNumeric(993, user->nick, InspIRCd::Format("Added %s to DCCALLOW list for %ld seconds", target->nick.c_str(), length));
					}
					else
					{
						user->WriteNumeric(994, user->nick, InspIRCd::Format("Added %s to DCCALLOW list for this session", target->nick.c_str()));
					}

					/* route it. */
					return CMD_SUCCESS;
				}
			}
			else
			{
				// nick doesn't exist
				user->WriteNumeric(Numerics::NoSuchNick(nick));
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
		for (size_t i = 0; i < sizeof(helptext)/sizeof(helptext[0]); i++)
			user->WriteNumeric(998, helptext[i]);
		user->WriteNumeric(999, "End of DCCALLOW HELP");

		LocalUser* localuser = IS_LOCAL(user);
		if (localuser)
			localuser->CommandFloodPenalty += 4000;
	}

	void DisplayDCCAllowList(User* user)
	{
		 // display current DCCALLOW list
		user->WriteNumeric(990, "Users on your DCCALLOW list:");

		dl = ext.get(user);
		if (dl)
		{
			for (dccallowlist::const_iterator c = dl->begin(); c != dl->end(); ++c)
			{
				user->WriteNumeric(991, user->nick, InspIRCd::Format("%s (%s)", c->nickname.c_str(), c->hostmask.c_str()));
			}
		}

		user->WriteNumeric(992, "End of DCCALLOW list");
	}

};

class ModuleDCCAllow : public Module
{
	DCCAllowExt ext;
	CommandDccallow cmd;

 public:
	ModuleDCCAllow()
		: ext("dccallow", ExtensionItem::EXT_USER, this)
		, cmd(this, ext)
	{
	}

	void OnUserQuit(User* user, const std::string &reason, const std::string &oper_message) CXX11_OVERRIDE
	{
		dccallowlist* udl = ext.get(user);

		// remove their DCCALLOW list if they have one
		if (udl)
			stdalgo::erase(ul, user);

		// remove them from any DCCALLOW lists
		// they are currently on
		RemoveNick(user);
	}

	void OnUserPostNick(User* user, const std::string &oldnick) CXX11_OVERRIDE
	{
		RemoveNick(user);
	}

	ModResult OnUserPreMessage(User* user, void* dest, int target_type, std::string& text, char status, CUList& exempt_list, MessageType msgtype) CXX11_OVERRIDE
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
					dl = ext.get(u);
					if (dl && dl->size())
					{
						for (dccallowlist::const_iterator iter = dl->begin(); iter != dl->end(); ++iter)
							if (InspIRCd::Match(user->GetFullHost(), iter->hostmask))
								return MOD_RES_PASSTHRU;
					}

					std::string buf = text.substr(5);
					size_t s = buf.find(' ');
					if (s == std::string::npos)
						return MOD_RES_PASSTHRU;

					const std::string type = buf.substr(0, s);

					ConfigTag* conftag = ServerInstance->Config->ConfValue("dccallow");
					bool blockchat = conftag->getBool("blockchat");

					if (stdalgo::string::equalsci(type, "SEND"))
					{
						size_t first;

						buf = buf.substr(s + 1);

						if (!buf.empty() && buf[0] == '"')
						{
							s = buf.find('"', 1);

							if (s == std::string::npos || s <= 1)
								return MOD_RES_PASSTHRU;

							--s;
							first = 1;
						}
						else
						{
							s = buf.find(' ');
							first = 0;
						}

						if (s == std::string::npos)
							return MOD_RES_PASSTHRU;

						std::string defaultaction = conftag->getString("action");
						std::string filename = buf.substr(first, s);

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

						user->WriteNotice("The user " + u->nick + " is not accepting DCC SENDs from you. Your file " + filename + " was not sent.");
						u->WriteNotice(user->nick + " (" + user->ident + "@" + user->dhost + ") attempted to send you a file named " + filename + ", which was blocked.");
						u->WriteNotice("If you trust " + user->nick + " and were expecting this, you can type /DCCALLOW HELP for information on the DCCALLOW system.");
						return MOD_RES_DENY;
					}
					else if ((blockchat) && (stdalgo::string::equalsci(type, "CHAT")))
					{
						user->WriteNotice("The user " + u->nick + " is not accepting DCC CHAT requests from you.");
						u->WriteNotice(user->nick + " (" + user->ident + "@" + user->dhost + ") attempted to initiate a DCC CHAT session, which was blocked.");
						u->WriteNotice("If you trust " + user->nick + " and were expecting this, you can type /DCCALLOW HELP for information on the DCCALLOW system.");
						return MOD_RES_DENY;
					}
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}

	void Expire()
	{
		for (userlist::iterator iter = ul.begin(); iter != ul.end();)
		{
			User* u = (User*)(*iter);
			dl = ext.get(u);
			if (dl)
			{
				if (dl->size())
				{
					dccallowlist::iterator iter2 = dl->begin();
					while (iter2 != dl->end())
					{
						if (iter2->length != 0 && (iter2->set_on + iter2->length) <= ServerInstance->Time())
						{
							u->WriteNumeric(997, u->nick, InspIRCd::Format("DCCALLOW entry for %s has expired", iter2->nickname.c_str()));
							iter2 = dl->erase(iter2);
						}
						else
						{
							++iter2;
						}
					}
				}
				++iter;
			}
			else
			{
				iter = ul.erase(iter);
			}
		}
	}

	void RemoveNick(User* user)
	{
		/* Iterate through all DCCALLOW lists and remove user */
		for (userlist::iterator iter = ul.begin(); iter != ul.end();)
		{
			User *u = (User*)(*iter);
			dl = ext.get(u);
			if (dl)
			{
				if (dl->size())
				{
					for (dccallowlist::iterator i = dl->begin(); i != dl->end(); ++i)
					{
						if (i->nickname == user->nick)
						{

							u->WriteNotice(i->nickname + " left the network or changed their nickname and has been removed from your DCCALLOW list");
							u->WriteNumeric(995, u->nick, InspIRCd::Format("Removed %s from your DCCALLOW list", i->nickname.c_str()));
							dl->erase(i);
							break;
						}
					}
				}
				++iter;
			}
			else
			{
				iter = ul.erase(iter);
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

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("dccallow");
		cmd.maxentries = tag->getInt("maxentries", 20);

		bfl.clear();
		ConfigTagList tags = ServerInstance->Config->ConfTags("banfile");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			BannedFileList bf;
			bf.filemask = i->second->getString("pattern");
			bf.action = i->second->getString("action");
			bfl.push_back(bf);
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support for the /DCCALLOW command", VF_COMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleDCCAllow)
