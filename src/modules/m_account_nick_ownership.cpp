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

typedef std::vector<NickTSItem> NicksOwned;

class NicksOwnedExtItem : public TSGenericExtItem<NicksOwned>
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
		NicksOwned* ext = get_value(entry);
		if(ext)
			for(NicksOwned::iterator iter = ext->begin(); iter != ext->end(); ++iter)
				if(iter->nick == nick)
				{
					if(iter->ts < ts || (iter->ts == ts && entry->name < accountname))
						return true;
					ext->erase(iter);
					nickinfo.erase(i);
					return false;
				}

		throw ModuleException("An entry in nickinfo is incorrect");
	}

 protected:
	virtual std::string value_serialize(SerializeFormat format, const NicksOwned* value) const
	{
		std::ostringstream str;
		if(format == FORMAT_USER)
		{
			for(NicksOwned::const_iterator i = value->begin(); i != value->end(); ++i)
			{
				str << i->nick << " ";
			}
			std::string retval = str.str();
			return retval.empty() ? "-none-" : retval;
		}
		for(NicksOwned::const_iterator i = value->begin(); i != value->end(); ++i)
		{
			str << i->nick << "," << i->ts << " ";
		}
		return str.str();
	}

 private:
	virtual NicksOwned* value_unserialize(SerializeFormat, const std::string&)
	{
		throw ModuleException("Called NicksOwnedExtItem::value_unserialize");
	}

	virtual void value_resolve_conflict(NicksOwned*, NicksOwned*)
	{
		throw ModuleException("Called NicksOwnedExtItem::value_resolve_conflict");
	}

 public:
	NicksOwnedExtItem(const std::string& Key, Module* parent) : TSGenericExtItem<NicksOwned>(Key, NicksOwned(), parent)
	{
	}

	virtual void unserialize(SerializeFormat format, Extensible* container, const std::string& value)
	{
		std::string::size_type delim = value.find_first_of(' ');
		time_t ts = atol(value.substr(0, delim).c_str());
		std::string item;
		if(delim != std::string::npos)
			item = value.substr(delim + 1);
		value_pair* p = get(container);
		if(!p || ts > p->first || (ts == p->first && item < value_serialize(format, p->second)))
		{
			if(p)
				for(NicksOwned::const_iterator i = p->second->begin(); i != p->second->end(); ++i)
					nickinfo.erase(i->nick);

			std::string token;
			irc::string nick;
			time_t nickTS;
			irc::spacesepstream sep(item);
			std::vector<std::pair<irc::string, AccountDBEntry*> > newinfo;
			NicksOwned* ptr = new NicksOwned;
			while(sep.GetToken(token))
			{
				delim = token.find_first_of(',');
				if(delim == std::string::npos)
					continue;
				nick = token.substr(0, delim);
				nickTS = atol(token.substr(delim + 1).c_str());
				if(!CheckCollision(static_cast<AccountDBEntry*>(container)->name, nick, nickTS))
				{
					newinfo.push_back(std::make_pair(nick, static_cast<AccountDBEntry*>(container)));
					ptr->push_back(NickTSItem(nick, nickTS));
				}
			}
			set(container, ts, ptr);
			nickinfo.insert(newinfo.begin(), newinfo.end());
		}
	}

	virtual void free(void* item)
	{
		if(!item) return;
		NicksOwned* value = static_cast<value_pair*>(item)->second;
		for(NicksOwned::const_iterator i = value->begin(); i != value->end(); ++i)
			nickinfo.erase(i->nick);
		this->TSGenericExtItem<NicksOwned>::free(item);
	}
};

static NicksOwnedExtItem* nicks_ext;

static void RemoveNick(const irc::string& nick)
{
	NickMap::iterator i = nickinfo.find(nick);
	NicksOwned* ext = nicks_ext->get_value(i->second);
	if(ext)
		for(NicksOwned::iterator iter = ext->begin(); iter != ext->end(); ++iter)
			if(iter->nick == nick)
			{
				ext->erase(iter);
				nickinfo.erase(i);
				return;
			}

	throw ModuleException("An entry in nickinfo is incorrect");
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
		NicksOwnedExtItem::value_pair* p = nicks_ext->get(entry);
		if(!p)
		{
			NicksOwned* value = new NicksOwned;
			value->push_back(NickTSItem(user->nick, ServerInstance->Time()));
			nicks_ext->set(entry, value);
		}
		else if(limit && p->second->size() >= limit)
		{
			user->WriteServ("NOTICE %s :You already have the maximum number of nicks registered", user->nick.c_str());
			return CMD_FAILURE;
		}
		else
		{
			p->first = ServerInstance->Time();
			p->second->push_back(NickTSItem(user->nick, ServerInstance->Time()));
		}
		nickinfo.insert(std::make_pair(user->nick, entry));
		db->SendUpdate(entry, "Nicks_owned");
		ServerInstance->SNO->WriteGlobalSno('u', "%s used ADDNICK to register their nick to %s", user->nick.c_str(), entry->name.c_str());
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
		NicksOwnedExtItem::value_pair* p = nicks_ext->get(entry);
		if(p)
			for(NicksOwned::iterator i = p->second->begin(); i != p->second->end(); ++i)
				if(i->nick == nick)
				{
					p->first = ServerInstance->Time();
					p->second->erase(i);
					nickinfo.erase(iter);
					db->SendUpdate(entry, "Nicks_owned");
					ServerInstance->SNO->WriteGlobalSno('u', "%s used DELNICK to unregister nick %s from %s", user->nick.c_str(), nick.c_str(), entry->name.c_str());
					user->WriteServ("NOTICE %s :Nick %s has been unregistered", user->nick.c_str(), nick.c_str());
					return CMD_SUCCESS;
				}

		throw ModuleException("An entry in nickinfo is incorrect");
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
		NicksOwnedExtItem::value_pair* p = nicks_ext->get(entry);
		if(p)
			for(NicksOwned::iterator i = p->second->begin(); i != p->second->end(); ++i)
				if(i->nick == parameters[1])
				{
					p->first = ServerInstance->Time();
					p->second->erase(i);
					nickinfo.erase(iter);
					db->SendUpdate(entry, "Nicks_owned");
					ServerInstance->SNO->WriteGlobalSno('a', "%s used FDELNICK to unregister nick %s from account %s", user->nick.c_str(), parameters[1].c_str(), entry->name.c_str());
					user->WriteServ("NOTICE %s :Nick %s has been unregistered from account %s", user->nick.c_str(), parameters[1].c_str(), entry->name.c_str());
					return CMD_SUCCESS;
				}

		throw ModuleException("An entry in nickinfo is incorrect");
	}
};

/** Handle /SETENFORCE
 */
class CommandSetenforce : public Command
{
 public:
	TSBoolExtItem enforce;
	CommandSetenforce(Module* Creator) : Command(Creator,"SETENFORCE", 1, 1), enforce("Automatically_enforce_nicks", false, true, Creator)
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
		enforce.set(entry, newsetting);
		db->SendUpdate(entry, "Automatically_enforce_nicks");
		user->WriteServ("NOTICE %s :Nick enforcement for account %s %s successfully", user->nick.c_str(), entry->name.c_str(), newsetting ? "enabled" : "disabled");
		return CMD_SUCCESS;
	}
};

/** Handle /ENFORCE
 */
class CommandEnforce : public Command
{
 public:
	CommandEnforce(Module* Creator) : Command(Creator,"ENFORCE", 1, 1)
	{
		syntax = "<nick>";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		if(IS_LOCAL(user))
		{
			AccountDBEntry* entry;
			if(!accounts || !accounts->IsRegistered(user) || !(entry = db->GetAccount(accounts->GetAccountName(user), false)))
			{
				user->WriteServ("NOTICE %s :You are not logged in", user->nick.c_str());
				return CMD_FAILURE;
			}
			NickMap::iterator iter = nickinfo.find(parameters[0]);
			if(entry->name != parameters[0] && (iter == nickinfo.end() || iter->second != entry))
			{
				user->WriteServ("NOTICE %s :Nick %s is not registered to you", user->nick.c_str(), parameters[0].c_str());
				return CMD_FAILURE;
			}
		}
		User* target = ServerInstance->FindNick(parameters[0]);
		if(!target)
		{
			user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}
		if(IS_LOCAL(user))
			user->WriteServ("NOTICE %s :Nick %s enforced successfully", user->nick.c_str(), parameters[0].c_str());
		if(IS_LOCAL(target))
		{
			target->WriteNumeric(433, "%s %s :Nickname overruled: ENFORCE command used",
				target->nick.c_str(), target->nick.c_str());
			target->ChangeNick(target->uuid, true);
			if(user->registered != REG_ALL)
				user->registered &= ~REG_NICK;
		}
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);
		if (dest)
			return ROUTE_OPT_UCAST(dest->server);
		return ROUTE_LOCALONLY;
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
	CommandEnforce cmd_enforce;

	ModuleAccountNickOwnership() : nicks("Nicks_owned", this), cmd_addnick(this), cmd_delnick(this), cmd_fdelnick(this), cmd_setenforce(this), cmd_enforce(this)
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
		ServerInstance->Modules->AddService(cmd_enforce);
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
		bool* enforce = cmd_setenforce.enforce.get_value(owner);
		if (!enforce || !*enforce)
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
		bool* enforce = cmd_setenforce.enforce.get_value(owner);
		if (enforce && *enforce && (!accounts || owner->name != accounts->GetAccountName(user)))
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
		bool* enforce = cmd_setenforce.enforce.get_value(owner);
		if ((!enforce || !*enforce) && (!accounts || owner->name != accounts->GetAccountName(user)))
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
				NicksOwned* ext = nicks.get_value(iter->second);
				if(ext)
					for(NicksOwned::const_iterator i = ext->begin(); i != ext->end(); ++i)
						if(i->nick == e.account)
						{
							e.alias_ts = i->ts;
							e.RemoveAliasImpl = &RemoveNick;
							return;
						}

				throw ModuleException("An entry in nickinfo is incorrect");
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
