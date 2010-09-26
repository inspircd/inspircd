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
#include "hash.h"

/* $ModDesc: Allow users to register their own accounts */

static dynamic_reference<AccountProvider> account("account");
static dynamic_reference<AccountDBProvider> db("accountdb");

/* XXX: Do we want to put these somewhere global? */
class TSExtItem : public SimpleExtItem<time_t>
{
 public:
	TSExtItem(const std::string& Key, Module* parent) : SimpleExtItem<time_t>(EXTENSIBLE_ACCOUNT, Key, parent) {}
	std::string serialize(SerializeFormat format, const Extensible* container, void* item) const
	{
		time_t* ts = static_cast<time_t*>(item);
		if(!ts) /* If we don't have a TS, not if the TS is zero */
			return "";
		return ConvToStr(*ts);
	}

	void unserialize(SerializeFormat format, Extensible* container, const std::string& value)
	{
		time_t* ours = get(container);
		time_t theirs = atol(value.c_str());
		if(!ours || theirs > *ours)
			set(container, theirs);
	}
};

class TSBoolExtItem : public SimpleExtItem<std::pair<time_t, bool> >
{
 public:
	TSBoolExtItem(const std::string& Key, Module* parent) : SimpleExtItem<std::pair<time_t, bool> >(EXTENSIBLE_ACCOUNT, Key, parent) {}
	std::string serialize(SerializeFormat format, const Extensible* container, void* item) const
	{
		std::pair<time_t, bool>* p = static_cast<std::pair<time_t, bool>*>(item);
		if(!p)
			return "";
		return ConvToStr(p->first) + (format == FORMAT_NETWORK ? " :" : " ") + (p->second ? '1' : '0');
	}

	void unserialize(SerializeFormat format, Extensible* container, const std::string& value)
	{
		time_t ts;
		bool item;
		std::string::size_type delim = value.find_first_of(' ');
		ts = atol(value.substr(0, delim).c_str());
		if(delim == std::string::npos)
			item = false;
		else
			item = (value.substr(delim + 1)[0] == '1');
		std::pair<time_t, bool>* p = get(container);
		if(!p || ts > p->first)
			set(container, std::make_pair(ts, item));
	}
};

/** Handle /REGISTER
 */
class CommandRegister : public Command
{
	const std::string& hashtype;
 public:
	CommandRegister(Module* Creator, const std::string& hashtype_ref) : Command(Creator,"REGISTER", 1, 1), hashtype(hashtype_ref)
	{
		syntax = "<password>";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		if (!ServerInstance->IsNick(user->nick.c_str(), ServerInstance->Config->Limits.NickMax))
		{
			user->WriteServ("NOTICE " + user->nick + " :You may not register your UID");
			return CMD_FAILURE;
		}
		if (account->IsRegistered(user))
		{
			user->WriteServ("NOTICE " + user->nick + " :You are already logged in to an account");
			return CMD_FAILURE;
		}
		AccountDBEntry* entry = db->GetAccount(user->nick);
		if(entry)
		{
			user->WriteServ("NOTICE " + user->nick + " :An account with name " + user->nick + " already exists");
			return CMD_FAILURE;
		}
		entry = new AccountDBEntry(user->nick, ServerInstance->Time());
		entry->hash_password_ts = entry->connectclass_ts = entry->tag_ts = entry->ts;
		entry->hash = hashtype;
		// XXX: The code to generate a hash was copied from m_password_hash.  We may want a better way to do this.
		if (hashtype == "plaintext" || hashtype.empty())
				entry->password = parameters[0];
		else if (hashtype.substr(0,5) == "hmac-")
		{
			std::string type = hashtype.substr(5);
			HashProvider* hp = ServerInstance->Modules->FindDataService<HashProvider>("hash/" + type);
			if (!hp)
			{
				entry->hash = "plaintext";
				entry->password = parameters[0];
				ServerInstance->Logs->Log ("MODULE", DEFAULT, "unknown hash type in m_account_register, not using a hash");
			}
			else
			{
				std::string salt = ServerInstance->GenRandomStr(6, false);
				entry->password = BinToBase64(salt) + "$" + BinToBase64(hp->hmac(salt, parameters[0]), NULL, 0);
			}
		}
		else
		{
			HashProvider* hp = ServerInstance->Modules->FindDataService<HashProvider>("hash/" + hashtype);
			if (hp)
			{
				entry->password = hp->hexsum(parameters[0]);
			}
			else
			{
				entry->hash = "plaintext";
				entry->password = parameters[0];
				ServerInstance->Logs->Log ("MODULE", DEFAULT, "unknown hash type in m_account_register, not using a hash");
			}
		}
		db->AddAccount(entry, true);

		if(account) account->DoLogin(user, entry->name, entry->tag);
		if(!entry->connectclass.empty()) ServerInstance->ForcedClass.set(user, entry->connectclass);
		return CMD_SUCCESS;
	}
};

/** Handle /CHGPASS
 */
class CommandChgpass : public Command
{
	const std::string& hashtype;
 public:
	CommandChgpass(Module* Creator, const std::string& hashtype_ref) : Command(Creator,"CHGPASS", 2, 3), hashtype(hashtype_ref)
	{
		syntax = "[username] <old password> <new password>";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		irc::string username;
		std::string oldpass, newpass;
		if(parameters.size() == 2)
		{
			if(account && account->IsRegistered(user))
				username = account->GetAccountName(user);
			else
				username = user->nick;
			oldpass = parameters[0];
			newpass = parameters[1];
		}
		else
		{
			username = parameters[0];
			oldpass = parameters[1];
			newpass = parameters[2];
		}
		if(newpass.empty())
		{
			user->WriteServ("NOTICE " + user->nick + " :You must specify a new password");
			return CMD_FAILURE;
		}
		AccountDBEntry* entry = db->GetAccount(username);
		if(!entry || entry->password.empty() || ServerInstance->PassCompare(user, entry->password, oldpass, entry->hash))
		{
			user->WriteServ("NOTICE " + user->nick + " :Invalid username or password");
			return CMD_FAILURE;
		}
		entry->hash = hashtype;
		// XXX: The code to generate a hash was copied from m_password_hash.  We may want a better way to do this.
		if (hashtype == "plaintext" || hashtype.empty())
				entry->password = newpass;
		else if (hashtype.substr(0,5) == "hmac-")
		{
			std::string type = hashtype.substr(5);
			HashProvider* hp = ServerInstance->Modules->FindDataService<HashProvider>("hash/" + type);
			if (!hp)
			{
				entry->hash = "plaintext";
				entry->password = newpass;
				ServerInstance->Logs->Log ("MODULE", DEFAULT, "unknown hash type in m_account_register, not using a hash");
			}
			else
			{
				std::string salt = ServerInstance->GenRandomStr(6, false);
				entry->password = BinToBase64(salt) + "$" + BinToBase64(hp->hmac(salt, newpass), NULL, 0);
			}
		}
		else
		{
			HashProvider* hp = ServerInstance->Modules->FindDataService<HashProvider>("hash/" + hashtype);
			if (hp)
			{
				entry->password = hp->hexsum(newpass);
			}
			else
			{
				entry->hash = "plaintext";
				entry->password = newpass;
				ServerInstance->Logs->Log ("MODULE", DEFAULT, "unknown hash type in m_account_register, not using a hash");
			}
		}
		entry->hash_password_ts = ServerInstance->Time();
		db->SendUpdate(entry, "hash_password");
		user->WriteServ("NOTICE " + user->nick + " :Account " + std::string(entry->name) + " password changed successfully");
		return CMD_SUCCESS;
	}
};

/** Handle /FCHGPASS
 */
class CommandFchgpass : public Command
{
	const std::string& hashtype;
 public:
	CommandFchgpass(Module* Creator, const std::string& hashtype_ref) : Command(Creator,"FCHGPASS", 2, 2), hashtype(hashtype_ref)
	{
		flags_needed = 'o'; syntax = "<username> <new password>";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		AccountDBEntry* entry = db->GetAccount(parameters[0]);
		if(!entry)
		{
			user->WriteServ("NOTICE " + user->nick + " :No such account");
			return CMD_FAILURE;
		}
		if(parameters[1].empty())
		{
			user->WriteServ("NOTICE " + user->nick + " :You must specify a new password");
			return CMD_FAILURE;
		}
		entry->hash = hashtype;
		// XXX: The code to generate a hash was copied from m_password_hash.  We may want a better way to do this.
		if (hashtype == "plaintext" || hashtype.empty())
				entry->password = parameters[1];
		else if (hashtype.substr(0,5) == "hmac-")
		{
			std::string type = hashtype.substr(5);
			HashProvider* hp = ServerInstance->Modules->FindDataService<HashProvider>("hash/" + type);
			if (!hp)
			{
				entry->hash = "plaintext";
				entry->password = parameters[1];
				ServerInstance->Logs->Log ("MODULE", DEFAULT, "unknown hash type in m_account_register, not using a hash");
			}
			else
			{
				std::string salt = ServerInstance->GenRandomStr(6, false);
				entry->password = BinToBase64(salt) + "$" + BinToBase64(hp->hmac(salt, parameters[1]), NULL, 0);
			}
		}
		else
		{
			HashProvider* hp = ServerInstance->Modules->FindDataService<HashProvider>("hash/" + hashtype);
			if (hp)
			{
				entry->password = hp->hexsum(parameters[1]);
			}
			else
			{
				entry->hash = "plaintext";
				entry->password = parameters[1];
				ServerInstance->Logs->Log ("MODULE", DEFAULT, "unknown hash type in m_account_register, not using a hash");
			}
		}
		entry->hash_password_ts = ServerInstance->Time();
		db->SendUpdate(entry, "hash_password");
		ServerInstance->SNO->WriteGlobalSno('a', "%s used FCHGPASS to force password change of account '%s'", user->nick.c_str(), entry->name.c_str());
		user->WriteServ("NOTICE " + user->nick + " :Account " + std::string(entry->name) + " force-password changed successfully");
		return CMD_SUCCESS;
	}
};

/** Handle /DROP
 */
class CommandDrop : public Command
{
 public:
	CommandDrop(Module* Creator) : Command(Creator,"DROP", 1, 2)
	{
		syntax = "[account name] <password>";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		irc::string username;
		std::string password;
		if(parameters.size() == 1)
		{
			if(account && account->IsRegistered(user))
				username = account->GetAccountName(user);
			else
				username = user->nick;
			password = parameters[0];
		}
		else
		{
			username = parameters[0];
			password = parameters[1];
		}
		AccountDBEntry* entry = db->GetAccount(username);
		if(!entry || entry->password.empty() || ServerInstance->PassCompare(user, entry->password, password, entry->hash))
		{
			user->WriteServ("NOTICE " + user->nick + " :Invalid username or password");
			return CMD_FAILURE;
		}
		if(account) account->DoLogin(user, "", "");
		user->WriteServ("NOTICE " + user->nick + " :Account " + std::string(entry->name) + " dropped successfully");
		db->RemoveAccount(entry, true);
		entry->cull();
		delete entry;
		return CMD_SUCCESS;
	}
};

/** Handle /FDROP
 */
class CommandFdrop : public Command
{
 public:
	CommandFdrop(Module* Creator) : Command(Creator,"FDROP", 1, 1)
	{
		flags_needed = 'o'; syntax = "<account name>";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		AccountDBEntry* entry = db->GetAccount(parameters[0]);
		if(!entry)
		{
			user->WriteServ("NOTICE " + user->nick + " :No such account");
			return CMD_FAILURE;
		}
		ServerInstance->SNO->WriteGlobalSno('a', "%s used FDROP to force drop of account '%s'", user->nick.c_str(), entry->name.c_str());
		user->WriteServ("NOTICE " + user->nick + " :Account " + std::string(entry->name) + " force-dropped successfully");
		db->RemoveAccount(entry, true);
		entry->cull();
		delete entry;
		return CMD_SUCCESS;
	}
};

/** Handle /HOLD
 */
class CommandHold : public Command
{
 public:
	TSBoolExtItem held;
	CommandHold(Module* Creator) : Command(Creator,"HOLD", 2, 2), held("held", Creator)
	{
		flags_needed = 'o'; syntax = "<account name> OFF|ON";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		bool newsetting;
		if(irc::string(parameters[1]) == "ON")
			newsetting = true;
		else if(irc::string(parameters[1]) == "OFF")
			newsetting = false;
		else
		{
			user->WriteServ("NOTICE " + user->nick + " :Unknown setting");
			return CMD_FAILURE;
		}
		AccountDBEntry* entry = db->GetAccount(parameters[0]);
		if(!entry)
		{
			user->WriteServ("NOTICE " + user->nick + " :No such account");
			return CMD_FAILURE;
		}
		held.set(entry, std::make_pair(ServerInstance->Time(), newsetting));
		db->SendUpdate(entry, "held");
		ServerInstance->SNO->WriteGlobalSno('a', "%s used HOLD to %sable hold of account '%s'", user->nick.c_str(), newsetting ? "en" : "dis", entry->name.c_str());
		user->WriteServ("NOTICE " + user->nick + " :Account " + std::string(entry->name) + (newsetting ? " held" : " unheld") + " successfully");
		return CMD_SUCCESS;
	}
};

class ModuleAccountRegister : public Module
{
	time_t expiretime;
	std::string hashtype;
	CommandRegister cmd_register;
	CommandChgpass cmd_chgpass;
	CommandFchgpass cmd_fchgpass;
	CommandDrop cmd_drop;
	CommandFdrop cmd_fdrop;
	CommandHold cmd_hold;
	TSExtItem last_used;

 public:
	ModuleAccountRegister() : cmd_register(this, hashtype), cmd_chgpass(this, hashtype), cmd_fchgpass(this, hashtype),
		cmd_drop(this), cmd_fdrop(this), cmd_hold(this), last_used("last_used", this)
	{
	}

	void init()
	{
		if(!db) throw ModuleException("m_account_register requires that m_account be loaded");
		ServerInstance->Modules->AddService(cmd_register);
		ServerInstance->Modules->AddService(cmd_chgpass);
		ServerInstance->Modules->AddService(cmd_fchgpass);
		ServerInstance->Modules->AddService(cmd_drop);
		ServerInstance->Modules->AddService(cmd_fdrop);
		ServerInstance->Modules->AddService(cmd_hold);
		ServerInstance->Modules->AddService(cmd_hold.held);
		ServerInstance->Modules->AddService(last_used);
		Implementation eventlist[] = { I_OnEvent, I_OnGarbageCollect };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void ReadConfig(ConfigReadStatus&)
	{
		ConfigTag* conf = ServerInstance->Config->GetTag("acctregister");
		hashtype = conf->getString("hashtype", "plaintext");
		expiretime = ServerInstance->Duration (conf->getString ("expiretime", "21d"));
	}

	void OnEvent(Event& event)
	{
		if(event.id == "account_login"){
			AccountEvent& acct_event = static_cast<AccountEvent&>(event);
			if(!IS_LOCAL(acct_event.user))
				return;
			AccountDBEntry* entry = db->GetAccount(acct_event.account);
			if(!entry)
				return;
			last_used.set(entry, ServerInstance->Time());
			db->SendUpdate(entry, "last_used");
		}
	}

	void OnGarbageCollect()
	{
		std::string account_name;
		AccountDBEntry* entry;
		time_t threshold = ServerInstance->Time() - expiretime;
		for (user_hash::const_iterator i = ServerInstance->Users->clientlist->begin(); i != ServerInstance->Users->clientlist->end(); ++i)
		{
			account_name = account->GetAccountName(i->second);
			if(account_name.empty())
				continue;
			entry = db->GetAccount(account_name);
			if(!entry)
				continue;
			last_used.set(entry, ServerInstance->Time()); // ServerInstance->Time() is inlined, so we would gain nothing by using a temporary variable
			db->SendUpdate(entry, "last_used");
		}
		for (AccountDB::const_iterator i = db->GetDB().begin(); i != db->GetDB().end();)
		{
			entry = i++->second;
			if(*last_used.get(entry) < threshold)
			{
				db->RemoveAccount(entry, true);
				entry->cull();
				delete entry;
			}
		}
	}
	void Prioritize()
	{
		ServerInstance->Modules->SetPriority(this, I_ModuleInit, PRIORITY_AFTER, ServerInstance->Modules->Find("m_account.so"));
	}

	Version GetVersion()
	{
		return Version("Allow users to register their own accounts", VF_VENDOR);
	}
};

MODULE_INIT(ModuleAccountRegister)
