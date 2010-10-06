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

class CommandAddnick : public Command
{
	NicksOwnedExtItem& nicks;
 public:
	CommandAddnick(Module* parent, NicksOwnedExtItem& nicks_ref) : Command(parent, "ADDNICK", 0, 0), nicks(nicks_ref)
	{
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User* user)
	{
		AccountDBEntry* entry;
		if(nickinfo.find(user->nick) != nickinfo.end())
		{
			user->WriteServ("NOTICE " + user->nick + " :Nick " + user->nick + " is already registered");
			return CMD_FAILURE;
		}
		if(!accounts || !accounts->IsRegistered(user) || !(entry = db->GetAccount(accounts->GetAccountName(user))))
		{
			user->WriteServ("NOTICE " + user->nick + " :You are not logged in");
			return CMD_FAILURE;
		}
		nickinfo.insert(std::make_pair(user->nick, entry));
		NicksOwned* p = nicks.get(entry);
		bool needToSet = false;
		if(!p)
		{
			p = new NicksOwned;
			needToSet = true;
		}
		p->first = ServerInstance->Time();
		p->second.push_back(NickTSItem(user->nick, ServerInstance->Time()));
		if(needToSet)
			nicks.set(entry, p);
		db->SendUpdate(entry, "nicks");
		user->WriteServ("NOTICE " + user->nick + " :Nick " + user->nick + " has been registered to account " + std::string(entry->name));
		return CMD_SUCCESS;
	}
};

class CommandDelnick : public Command
{
	NicksOwnedExtItem& nicks;
 public:
	CommandDelnick(Module* parent, NicksOwnedExtItem& nicks_ref) : Command(parent, "DELNICK", 0, 1), nicks(nicks_ref)
	{
		syntax = "[nick]";
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User* user)
	{
		AccountDBEntry* entry;
		irc::string nick = parameters.size() ? parameters[0] : user->nick;
		if(!accounts || !accounts->IsRegistered(user) || !(entry = db->GetAccount(accounts->GetAccountName(user))))
		{
			user->WriteServ("NOTICE " + user->nick + " :You are not logged in");
			return CMD_FAILURE;
		}
		NickMap::iterator iter = nickinfo.find(nick);
		if(iter == nickinfo.end() || iter->second != entry)
		{
			user->WriteServ("NOTICE " + std::string(user->nick) + " :Nick " + std::string(nick) + " is not registered to you");
			return CMD_FAILURE;
		}
		nickinfo.erase(nick);
		NicksOwned* p = nicks.get(entry);
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
		user->WriteServ("NOTICE " + user->nick + " :Nick " + std::string(nick) + " has been unregistered");
		return CMD_SUCCESS;
	}
};

class ModuleNickRegister : public Module
{
 public:
	NicksOwnedExtItem nicks;
	CommandAddnick cmd_addnick;
	CommandDelnick cmd_delnick;

	ModuleNickRegister() : nicks("nicks", this), cmd_addnick(this, nicks), cmd_delnick(this, nicks) {}

	void init()
	{
		ServerInstance->Modules->AddService(nicks);
		ServerInstance->Modules->AddService(cmd_addnick);
		ServerInstance->Modules->AddService(cmd_delnick);
		Implementation eventlist[] = { I_OnUserPreNick, I_OnCheckReady };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	ModResult OnUserPreNick(User* user, const std::string& nick)
	{
		if (ServerInstance->NICKForced.get(user))
			return MOD_RES_PASSTHRU;

		// check the new nick
		NickMap::iterator iter = nickinfo.find(nick);
		if(iter == nickinfo.end())
			return MOD_RES_PASSTHRU;
		if (accounts && accounts->GetAccountName(user) == iter->second->name)
			return MOD_RES_PASSTHRU;
		if (user->registered == REG_ALL)
		{
			user->WriteNumeric(433, "%s %s :You must be identified to the account '%s' to use this nick",
				user->nick.c_str(), nick.c_str(), iter->second->name.c_str());
			return MOD_RES_DENY;
		}
		else
		{
			// allow through to give things like SASL or SQLauth time to return before denying
#if 0
			// this numeric makes both irssi and xchat auto-select a different nick, for now we'll send a NOTICE instead
			user->WriteNumeric(437, "%s %s :This nick requires you to identify to the account '%s'",
				user->nick.c_str(), nick.c_str(), iter->second->name.c_str());
#else
			user->WriteServ("NOTICE " + user->nick + " :Nick " + nick + " requires you to identify to the account '" + iter->second->name.c_str() + "'");
#endif
			return MOD_RES_PASSTHRU;
		}
	}

	ModResult OnCheckReady(LocalUser* user)
	{
		NickMap::iterator iter = nickinfo.find(user->nick);
		if(iter == nickinfo.end())
			return MOD_RES_PASSTHRU;
		if (!accounts || iter->second->name != accounts->GetAccountName(user))
		{
			user->WriteNumeric(433, "%s %s :Nickname overruled: requires login to the account '%s'",
				user->nick.c_str(), user->nick.c_str(), iter->second->name.c_str());
			user->ChangeNick(user->uuid, true);
		}
		return MOD_RES_PASSTHRU;
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

MODULE_INIT(ModuleNickRegister)
