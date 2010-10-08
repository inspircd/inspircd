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
#include "account.h"

static dynamic_reference<AccountProvider> accounts("account");
static dynamic_reference<AccountDBProvider> db("accountdb");

typedef std::map<irc::string, AccountDBEntry*> NickMap;

static NickMap nickinfo;

struct NickTSItem
{
	irc::string nick;
	time_t ts;
	NickTSItem(const irc::string& newnick, time_t newTS) : nick(newnick), ts(newTS)
	{
	}
};

typedef std::pair<time_t, std::vector<NickTSItem> > NicksOwned;

class NicksOwnedExtItem : public SimpleExtItem<NicksOwned>
{
	bool CheckCollision(irc::string accountname, irc::string nick, time_t ts)
	{
		AccountDBEntry* owner = db->GetAccount(nick, false);
		if(owner)
		{
			if(owner->ts > ts)
			{
				db->RemoveAccount(false, owner);
				return false;
			}
			return true;
		}
		NickMap::iterator i = nickinfo.find(nick);
		if(i == nickinfo.end()) return false;
		AccountDBEntry* entry = i->second;
		NicksOwned* ext = get(entry);
		if(!ext)
			throw ModuleException("An entry in nickinfo is incorrect");

		std::vector<NickTSItem>::iterator iter;
		for(iter = ext->second.begin(); iter != ext->second.end(); ++iter)
			if(iter->nick == nick)
				break;

		if(iter == ext->second.end())
			throw ModuleException("An entry in nickinfo is incorrect");
		if(iter->ts < ts || (iter->ts == ts && entry->name < accountname)) return true;
		ext->second.erase(iter);
		nickinfo.erase(i);
		return false;
	}

 public:
	NicksOwnedExtItem(const std::string& Key, Module* parent) : SimpleExtItem<NicksOwned>(EXTENSIBLE_ACCOUNT, Key, parent) {}
	std::string serialize(SerializeFormat format, const Extensible* container, void* item) const
	{
		NicksOwned* p = static_cast<NicksOwned*>(item);
		if(!p)
			return "";
		std::ostringstream str;
		str << p->first << (format == FORMAT_NETWORK ? " :" : " ");
		for(std::vector<NickTSItem>::iterator i = p->second.begin(); i != p->second.end(); ++i)
		{
			str << i->nick << "," << i->ts << " ";
		}
		return str.str();
	}

	void unserialize(SerializeFormat format, Extensible* container, const std::string& value)
	{
		NicksOwned* newvalue = new NicksOwned;
		std::string item;
		std::string::size_type delim = value.find_first_of(' ');
		newvalue->first = atol(value.substr(0, delim).c_str());
		if(delim == std::string::npos)
			item = "";
		else
			item = value.substr(delim + 1);
		NicksOwned* p = get(container);
		if(!p || newvalue->first > p->first)
		{
			if(p)
				for(std::vector<NickTSItem>::iterator i = p->second.begin(); i != p->second.end(); ++i)
					nickinfo.erase(i->nick);

			std::string token;
			irc::string nick;
			time_t ts;
			irc::spacesepstream sep(item);
			std::vector<std::pair<irc::string, AccountDBEntry*> > newinfo;
			while(sep.GetToken(token))
			{
				delim = token.find_first_of(',');
				if(delim == std::string::npos) continue;
				nick = token.substr(0, delim);
				ts = atol(token.substr(delim + 1).c_str());
				if(!CheckCollision(static_cast<AccountDBEntry*>(container)->name, nick, ts))
				{
					newinfo.push_back(std::make_pair(nick, static_cast<AccountDBEntry*>(container)));
					newvalue->second.push_back(NickTSItem(nick, ts));
				}
			}
			set(container, newvalue);
			nickinfo.insert(newinfo.begin(), newinfo.end());
		}
		else
			delete newvalue;
	}

	virtual void free(void* item)
	{
		NicksOwned* p = static_cast<NicksOwned*>(item);
		for(std::vector<NickTSItem>::iterator i = p->second.begin(); i != p->second.end(); ++i)
			nickinfo.erase(i->nick);
		delete p;
	}
};

static NicksOwnedExtItem* nicks_ext;

static void RemoveNick(const irc::string& nick)
{
	NickMap::iterator i = nickinfo.find(nick);
	AccountDBEntry* entry = i->second;
	NicksOwned* ext = nicks_ext->get(entry);
	if(!ext)
		throw ModuleException("An entry in nickinfo is incorrect");

	std::vector<NickTSItem>::iterator iter;
	for(iter = ext->second.begin(); iter != ext->second.end(); ++iter)
		if(iter->nick == nick)
			break;

	if(iter == ext->second.end())
		throw ModuleException("An entry in nickinfo is incorrect");
	ext->second.erase(iter);
	nickinfo.erase(i);
}

class CommandAddnick : public Command
{
 public:
	unsigned int limit;
	CommandAddnick(Module* parent) : Command(parent, "ADDNICK", 0, 0)
	{
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User* user)
	{
		AccountDBEntry* entry;
		if(!accounts || !accounts->IsRegistered(user) || !(entry = db->GetAccount(accounts->GetAccountName(user), false)))
		{
			user->WriteServ("NOTICE %s :You are not logged in", user->nick.c_str());
			return CMD_FAILURE;
		}
		if (!ServerInstance->IsNick(user->nick.c_str(), ServerInstance->Config->Limits.NickMax))
		{
			user->WriteServ("NOTICE %s :You may not register your UID", user->nick.c_str());
			return CMD_FAILURE;
		}
		if(db->GetAccount(user->nick, true))
		{
			user->WriteServ("NOTICE %s :Nick %s is already registered", user->nick.c_str(), user->nick.c_str());
			return CMD_FAILURE;
		}
		NicksOwned* p = nicks_ext->get(entry);
		bool needToSet = false;
		if(!p)
		{
			p = new NicksOwned;
			needToSet = true;
		}
		else if(limit && p->second.size() >= limit)
		{
			user->WriteServ("NOTICE %s :You already have the maximum number of nicks registered", user->nick.c_str());
			return CMD_FAILURE;
		}
		p->first = ServerInstance->Time();
		p->second.push_back(NickTSItem(user->nick, ServerInstance->Time()));
		if(needToSet)
			nicks_ext->set(entry, p);
		nickinfo.insert(std::make_pair(user->nick, entry));
		db->SendUpdate(entry, "nicks");
		user->WriteServ("NOTICE %s :Nick %s has been registered to account %s", user->nick.c_str(), user->nick.c_str(), entry->name.c_str());
		return CMD_SUCCESS;
	}
};

class CommandDelnick : public Command
{
 public:
	CommandDelnick(Module* parent) : Command(parent, "DELNICK", 0, 1)
	{
		syntax = "[nick]";
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User* user)
	{
		AccountDBEntry* entry;
		irc::string nick = parameters.size() ? parameters[0] : user->nick;
		if(!accounts || !accounts->IsRegistered(user) || !(entry = db->GetAccount(accounts->GetAccountName(user), false)))
		{
			user->WriteServ("NOTICE %s :You are not logged in", user->nick.c_str());
			return CMD_FAILURE;
		}
		AccountDBEntry* owner = db->GetAccount(nick, false);
		NickMap::iterator iter = nickinfo.find(nick);
		if(owner == entry)
		{
			user->WriteServ("NOTICE %s :Nick %s is your primary nick and may not be deleted", user->nick.c_str(), nick.c_str());
			return CMD_FAILURE;
		}
		else if(owner || iter == nickinfo.end() || iter->second != entry)
		{
			user->WriteServ("NOTICE %s :Nick %s is not registered to you", user->nick.c_str(), nick.c_str());
			return CMD_FAILURE;
		}
		nickinfo.erase(iter);
		NicksOwned* p = nicks_ext->get(entry);
		if(!p)
			throw ModuleException("An entry in nickinfo is incorrect");
		p->first = ServerInstance->Time();
		std::vector<NickTSItem>::iterator i;
		for(i = p->second.begin(); i != p->second.end(); ++i)
			if(i->nick == nick)
				break;

		if(i == p->second.end())
			throw ModuleException("An entry in nickinfo is incorrect");
		p->second.erase(i);
		db->SendUpdate(entry, "nicks");
		user->WriteServ("NOTICE %s :Nick %s has been unregistered", user->nick.c_str(), nick.c_str());
		return CMD_SUCCESS;
	}
};

class CommandFdelnick : public Command
{
 public:
	CommandFdelnick(Module* Creator) : Command(Creator,"FDELNICK", 2, 2)
	{
		flags_needed = 'o'; syntax = "<account> <nick>";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		AccountDBEntry* entry = db->GetAccount(parameters[0], false);
		if(!entry)
		{
			user->WriteServ("NOTICE %s :No such account", user->nick.c_str());
			return CMD_FAILURE;
		}
		if(entry->name == parameters[1])
		{
			user->WriteServ("NOTICE %s :Nick %s is %s's primary nick and may not be deleted", user->nick.c_str(), entry->name.c_str(), entry->name.c_str());
			return CMD_FAILURE;
		}
		NickMap::iterator iter = nickinfo.find(parameters[1]);
		if(iter == nickinfo.end() || iter->second != entry)
		{
			user->WriteServ("NOTICE %s :Nick %s is not registered to them", user->nick.c_str(), parameters[1].c_str());
			return CMD_FAILURE;
		}
		nickinfo.erase(iter);
		NicksOwned* p = nicks_ext->get(entry);
		if(!p)
			throw ModuleException("An entry in nickinfo is incorrect");
		p->first = ServerInstance->Time();
		std::vector<NickTSItem>::iterator i;
		for(i = p->second.begin(); i != p->second.end(); ++i)
			if(i->nick == parameters[1])
				break;

		if(i == p->second.end())
			throw ModuleException("An entry in nickinfo is incorrect");
		p->second.erase(i);
		db->SendUpdate(entry, "nicks");
		ServerInstance->SNO->WriteGlobalSno('a', "%s used FDELNICK to unregister nick %s from account %s", user->nick.c_str(), parameters[1].c_str(), entry->name.c_str());
		user->WriteServ("NOTICE %s :Nick %s has been unregistered from account %s", user->nick.c_str(), parameters[1].c_str(), entry->name.c_str());
		return CMD_SUCCESS;
	}
};

/** Handle /SETENFORCE
 */
class CommandSetenforce : public Command
{
 public:
	TSBoolExtItem enforce;
	CommandSetenforce(Module* Creator) : Command(Creator,"SETENFORCE", 1, 1), enforce("enforce", Creator)
	{
		syntax = "OFF|ON";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		AccountDBEntry* entry;
		if(!accounts || !accounts->IsRegistered(user) || !(entry = db->GetAccount(accounts->GetAccountName(user), false)))
		{
			user->WriteServ("NOTICE %s :You are not logged in", user->nick.c_str());
			return CMD_FAILURE;
		}
		bool newsetting;
		if(irc::string(parameters[0]) == "ON")
			newsetting = true;
		else if(irc::string(parameters[0]) == "OFF")
			newsetting = false;
		else
		{
			user->WriteServ("NOTICE %s :Unknown setting", user->nick.c_str());
			return CMD_FAILURE;
		}
		enforce.set(entry, std::make_pair(ServerInstance->Time(), newsetting));
		db->SendUpdate(entry, "enforce");
		user->WriteServ("NOTICE %s :Nick enforcement for account %s %s successfully", user->nick.c_str(), entry->name.c_str(), newsetting ? "enabled" : "disabled");
		return CMD_SUCCESS;
	}
};

class ModuleAccountNickOwnership : public Module
{
 public:
	NicksOwnedExtItem nicks;
	CommandAddnick cmd_addnick;
	CommandDelnick cmd_delnick;
	CommandFdelnick cmd_fdelnick;
	CommandSetenforce cmd_setenforce;

	ModuleAccountNickOwnership() : nicks("nicks", this), cmd_addnick(this), cmd_delnick(this), cmd_fdelnick(this), cmd_setenforce(this)
	{
		nicks_ext = &nicks;
	}

	void init()
	{
		ServerInstance->Modules->AddService(nicks);
		ServerInstance->Modules->AddService(cmd_addnick);
		ServerInstance->Modules->AddService(cmd_delnick);
		ServerInstance->Modules->AddService(cmd_fdelnick);
		ServerInstance->Modules->AddService(cmd_setenforce);
		ServerInstance->Modules->AddService(cmd_setenforce.enforce);
		Implementation eventlist[] = { I_OnUserPreNick, I_OnCheckReady, I_OnUserConnect, I_OnEvent };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void ReadConfig(ConfigReadStatus&)
	{
		ConfigTag *tag = ServerInstance->Config->GetTag ("nickownership");
		cmd_addnick.limit = tag->getInt ("limit", 5);
	}

	ModResult OnUserPreNick(User* user, const std::string& nick)
	{
		if (ServerInstance->NICKForced.get(user))
			return MOD_RES_PASSTHRU;

		// check the new nick
		AccountDBEntry* owner = db->GetAccount(nick, true);
		if(!owner || (accounts && accounts->GetAccountName(user) == owner->name))
			return MOD_RES_PASSTHRU;
		std::pair<time_t, bool>* enforce = cmd_setenforce.enforce.get(owner);
		if (!enforce || !enforce->second)
		{
			if(user->registered == REG_ALL)
				user->WriteServ("NOTICE %s :Nick %s is registered to the account '%s'", user->nick.c_str(), nick.c_str(), owner->name.c_str());
			return MOD_RES_PASSTHRU;
		}
		else if (user->registered == REG_ALL)
		{
			user->WriteNumeric(433, "%s %s :You must be identified to the account '%s' to use this nick",
				user->nick.c_str(), nick.c_str(), owner->name.c_str());
			return MOD_RES_DENY;
		}
		else
		{
			// allow through to give things like SASL or SQLauth time to return before denying
			// sending a 437 here makes both irssi and xchat auto-select a different nick, so we'll send a NOTICE instead
			user->WriteServ("NOTICE %s :Nick %s requires you to identify to the account '%s'", user->nick.c_str(), nick.c_str(), owner->name.c_str());
			return MOD_RES_PASSTHRU;
		}
	}

	ModResult OnCheckReady(LocalUser* user)
	{
		AccountDBEntry* owner = db->GetAccount(user->nick, true);
		if(!owner)
			return MOD_RES_PASSTHRU;
		std::pair<time_t, bool>* enforce = cmd_setenforce.enforce.get(owner);
		if (enforce && enforce->second && (!accounts || owner->name != accounts->GetAccountName(user)))
		{
			user->WriteNumeric(433, "%s %s :Nickname overruled: requires login to the account '%s'",
				user->nick.c_str(), user->nick.c_str(), owner->name.c_str());
			user->ChangeNick(user->uuid, true);
			user->registered &= ~REG_NICK;
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	virtual void OnUserConnect(LocalUser* user)
	{
		AccountDBEntry* owner = db->GetAccount(user->nick, true);
		if(!owner)
			return;
		std::pair<time_t, bool>* enforce = cmd_setenforce.enforce.get(owner);
		if ((!enforce || !enforce->second) && (!accounts || owner->name != accounts->GetAccountName(user)))
			user->WriteServ("NOTICE %s :Nick %s is registered to the account '%s'", user->nick.c_str(), user->nick.c_str(), owner->name.c_str());
	}

	void OnEvent(Event& event)
	{
		if(event.id == "get_account_by_alias"){
			GetAccountByAliasEvent& e = static_cast<GetAccountByAliasEvent&>(event);
			if(e.entry)
				return; // Some other module already populated the event
			NickMap::iterator iter = nickinfo.find(e.account);
			if(iter != nickinfo.end())
			{
				e.entry = iter->second;
				NicksOwned* ext = nicks.get(iter->second);
				if(!ext)
					throw ModuleException("An entry in nickinfo is incorrect");

				std::vector<NickTSItem>::const_iterator i;
				for(i = ext->second.begin(); i != ext->second.end(); ++i)
					if(i->nick == e.account)
						break;

				if(i == ext->second.end())
					throw ModuleException("An entry in nickinfo is incorrect");
				e.alias_ts = i->ts;
				e.RemoveAliasImpl = &RemoveNick;
			}
		}
	}

	void Prioritize()
	{
		ServerInstance->Modules->SetPriority(this, I_ModuleInit, PRIORITY_AFTER, ServerInstance->Modules->Find("m_account.so"));
		// need to be after all modules that might deny the ready check
		ServerInstance->Modules->SetPriority(this, I_OnCheckReady, PRIORITY_LAST);
	}

	Version GetVersion()
	{
		return Version("Nick registration tracking", VF_VENDOR | VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleAccountNickOwnership)
