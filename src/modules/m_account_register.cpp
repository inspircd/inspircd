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

/** Handle /REGISTER
 */
class CommandRegister : public Command
{
	const std::string& hashtype;
	const std::set<irc::string>& recentlydropped;
	const unsigned int& maxregcount;
 public:
	LocalIntExt regcount;
	CommandRegister(Module* Creator, const std::string& hashtype_ref, const std::set<irc::string>& recentlydropped_ref, const unsigned int& maxregcount_ref) :
		Command(Creator,"REGISTER", 1, 1), hashtype(hashtype_ref), recentlydropped(recentlydropped_ref), maxregcount(maxregcount_ref), regcount(EXTENSIBLE_USER, "regcount", Creator)
	{
		syntax = "<password>";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		if (!ServerInstance->IsNick(user->nick.c_str(), ServerInstance->Config->Limits.NickMax))
		{
			user->WriteServ("NOTICE %s :You may not register your UID", user->nick.c_str());
			return CMD_FAILURE;
		}
		if (account && account->IsRegistered(user))
		{
			user->WriteServ("NOTICE %s :You are already logged in to an account", user->nick.c_str());
			return CMD_FAILURE;
		}
		if(recentlydropped.find(user->nick) != recentlydropped.end())
		{
			user->WriteServ("NOTICE %s :Account %s was dropped less than an hour ago and may not yet be re-registered", user->nick.c_str(), user->nick.c_str());
			return CMD_FAILURE;
		}
		unsigned int user_regcount = regcount.get(user);
		if(maxregcount && user_regcount >= maxregcount && !user->HasPrivPermission("accounts/no-registration-limit"))
		{
			user->WriteServ("NOTICE %s :You have already registered the maximum number of accounts for this session", user->nick.c_str());
			return CMD_FAILURE;
		}
		// Don't send this now.  Wait until we have the password set.
		AccountDBEntry* entry = db->AddAccount(false, user->nick, ServerInstance->Time(), hashtype);
		if(!entry)
		{
			user->WriteServ("NOTICE %s :Account %s already exists", user->nick.c_str(), user->nick.c_str());
			return CMD_FAILURE;
		}
		entry->hash_password_ts = entry->ts;
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
		db->SendAccount(entry);
		regcount.set(user, user_regcount + 1);
		ServerInstance->SNO->WriteGlobalSno('u', "%s used REGISTER to register a new account", user->nick.c_str());
		if(account) account->DoLogin(user, entry->name, "");
		return CMD_SUCCESS;
	}
};

/** Handle /SETPASS
 */
class CommandSetpass : public Command
{
	const std::string& hashtype;
 public:
	CommandSetpass(Module* Creator, const std::string& hashtype_ref) : Command(Creator,"SETPASS", 2, 3), hashtype(hashtype_ref)
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
			user->WriteServ("NOTICE %s :You must specify a new password", user->nick.c_str());
			return CMD_FAILURE;
		}
		AccountDBEntry* entry = db->GetAccount(username, false);
		if(!entry || entry->password.empty() || ServerInstance->PassCompare(user, entry->password, oldpass, entry->hash))
		{
			user->WriteServ("NOTICE %s :Invalid username or password", user->nick.c_str());
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
		user->WriteServ("NOTICE %s :Account %s password changed successfully", user->nick.c_str(), entry->name.c_str());
		return CMD_SUCCESS;
	}
};

/** Handle /FSETPASS
 */
class CommandFsetpass : public Command
{
	const std::string& hashtype;
 public:
	CommandFsetpass(Module* Creator, const std::string& hashtype_ref) : Command(Creator,"FSETPASS", 2, 2), hashtype(hashtype_ref)
	{
		flags_needed = 'o'; syntax = "<username> <new password>";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		AccountDBEntry* entry = db->GetAccount(parameters[0], false);
		if(!entry)
		{
			user->WriteServ("NOTICE %s :No such account", user->nick.c_str());
			return CMD_FAILURE;
		}
		if(parameters[1].empty())
		{
			user->WriteServ("NOTICE %s :You must specify a new password", user->nick.c_str());
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
		ServerInstance->SNO->WriteGlobalSno('a', "%s used FSETPASS to force password change of account '%s'", user->nick.c_str(), entry->name.c_str());
		user->WriteServ("NOTICE %s :Account %s force-password changed successfully", user->nick.c_str(), entry->name.c_str());
		return CMD_SUCCESS;
	}
};

/** Handle /DROP
 */
class CommandDrop : public Command
{
	std::set<irc::string>& recentlydropped;
 public:
	CommandDrop(Module* Creator, std::set<irc::string>& recentlydropped_ref) : Command(Creator,"DROP", 1, 2), recentlydropped(recentlydropped_ref)
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
		AccountDBEntry* entry = db->GetAccount(username, false);
		if(!entry || entry->password.empty() || ServerInstance->PassCompare(user, entry->password, password, entry->hash))
		{
			user->WriteServ("NOTICE %s :Invalid username or password", user->nick.c_str());
			return CMD_FAILURE;
		}
		if(!account || username != account->GetAccountName(user))
			user->WriteServ("NOTICE %s :Account %s has been dropped", user->nick.c_str(), username.c_str());
		recentlydropped.insert(entry->name);
		std::vector<std::string> params;
		params.push_back("*");
		params.push_back("RECENTLYDROPPED");
		params.push_back(entry->name);
		ServerInstance->PI->SendEncapsulatedData(params);
		ServerInstance->SNO->WriteGlobalSno('u', "%s used DROP to drop account %s", user->nick.c_str(), entry->name.c_str());
		db->RemoveAccount(true, entry);
		return CMD_SUCCESS;
	}
};

/** Handle /FDROP
 */
class CommandFdrop : public Command
{
	std::set<irc::string>& recentlydropped;
 public:
	CommandFdrop(Module* Creator, std::set<irc::string>& recentlydropped_ref) : Command(Creator,"FDROP", 1, 1), recentlydropped(recentlydropped_ref)
	{
		flags_needed = 'o'; syntax = "<account name>";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		AccountDBEntry* entry = db->GetAccount(parameters[0], false);
		if(!entry)
		{
			user->WriteServ("NOTICE %s :No such account", user->nick.c_str());
			return CMD_FAILURE;
		}
		ServerInstance->SNO->WriteGlobalSno('a', "%s used FDROP to force drop of account '%s'", user->nick.c_str(), entry->name.c_str());
		user->WriteServ("NOTICE %s :Account %s force-dropped successfully", user->nick.c_str(), entry->name.c_str());
		recentlydropped.insert(entry->name);
		std::vector<std::string> params;
		params.push_back("*");
		params.push_back("RECENTLYDROPPED");
		params.push_back(entry->name);
		ServerInstance->PI->SendEncapsulatedData(params);
		db->RemoveAccount(true, entry);
		return CMD_SUCCESS;
	}
};

/** Handle /HOLD
 */
class CommandHold : public Command
{
 public:
	TSBoolExtItem held;
	CommandHold(Module* Creator) : Command(Creator,"HOLD", 2, 2), held("held", false, Creator)
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
			user->WriteServ("NOTICE %s :Unknown setting", user->nick.c_str());
			return CMD_FAILURE;
		}
		AccountDBEntry* entry = db->GetAccount(parameters[0], false);
		if(!entry)
		{
			user->WriteServ("NOTICE %s :No such account", user->nick.c_str());
			return CMD_FAILURE;
		}
		held.set(entry, std::make_pair(ServerInstance->Time(), newsetting));
		db->SendUpdate(entry, "held");
		ServerInstance->SNO->WriteGlobalSno('a', "%s used HOLD to %sable hold of account '%s'", user->nick.c_str(), newsetting ? "en" : "dis", entry->name.c_str());
		user->WriteServ("NOTICE %s :Account %s %sheld successfully", user->nick.c_str(), entry->name.c_str(), newsetting ? "" : "un");
		return CMD_SUCCESS;
	}
};

/** Handle /RECENTLYDROPPED
 */
class CommandRecentlydropped : public Command
{
	std::set<irc::string>& recentlydropped;
 public:
	CommandRecentlydropped(Module* Creator, std::set<irc::string>& recentlydropped_ref) : Command(Creator,"RECENTLYDROPPED", 1, 1), recentlydropped(recentlydropped_ref)
	{
		flags_needed = FLAG_SERVERONLY; syntax = "<account name>";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User*)
	{
		recentlydropped.insert(parameters[0]);
		return CMD_SUCCESS;
	}
};

class ModuleAccountRegister : public Module
{
	time_t expiretime;
	std::string hashtype;
	std::set<irc::string> recentlydropped;
	unsigned int maxregcount;
	CommandRegister cmd_register;
	CommandSetpass cmd_setpass;
	CommandFsetpass cmd_fsetpass;
	CommandDrop cmd_drop;
	CommandFdrop cmd_fdrop;
	CommandHold cmd_hold;
	CommandRecentlydropped cmd_recentlydropped;
	TSExtItem last_used;

 public:
	ModuleAccountRegister() : cmd_register(this, hashtype, recentlydropped, maxregcount), cmd_setpass(this, hashtype),
		cmd_fsetpass(this, hashtype), cmd_drop(this, recentlydropped), cmd_fdrop(this, recentlydropped), cmd_hold(this),
		cmd_recentlydropped(this, recentlydropped), last_used("last_used", this)
	{
	}

	void init()
	{
		if(!db) throw ModuleException("m_account_register requires that m_account be loaded");
		ServerInstance->Modules->AddService(cmd_register);
		ServerInstance->Modules->AddService(cmd_register.regcount);
		ServerInstance->Modules->AddService(cmd_setpass);
		ServerInstance->Modules->AddService(cmd_fsetpass);
		ServerInstance->Modules->AddService(cmd_drop);
		ServerInstance->Modules->AddService(cmd_fdrop);
		ServerInstance->Modules->AddService(cmd_hold);
		ServerInstance->Modules->AddService(cmd_hold.held);
		ServerInstance->Modules->AddService(cmd_recentlydropped);
		ServerInstance->Modules->AddService(last_used);
		Implementation eventlist[] = { I_OnEvent, I_OnSyncNetwork, I_OnGarbageCollect };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void ReadConfig(ConfigReadStatus&)
	{
		ConfigTag* conf = ServerInstance->Config->GetTag("acctregister");
		hashtype = conf->getString("hashtype", "plaintext");
		expiretime = ServerInstance->Duration (conf->getString ("expiretime", "21d"));
		maxregcount = conf->getInt("maxregcount", 3);
		if(expiretime && expiretime < 7200)
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "account expiration times of under 2 hours are unsafe, setting to 2 hours");
			expiretime = 7200;
		}
	}

	void OnEvent(Event& event)
	{
		if(event.id == "account_login"){
			AccountEvent& acct_event = static_cast<AccountEvent&>(event);
			if(!IS_LOCAL(acct_event.user))
				return;
			AccountDBEntry* entry = db->GetAccount(acct_event.account, false);
			if(!entry)
				return;
			last_used.set(entry, ServerInstance->Time());
			db->SendUpdate(entry, "last_used");
		}
	}

	virtual void OnSyncNetwork(SyncTarget* target)
	{
		for (std::set<irc::string>::const_iterator i = recentlydropped.begin(); i != recentlydropped.end(); ++i)
			target->SendCommand("ENCAP * RECENTLYDROPPED " + i->value);
	}

	void OnGarbageCollect()
	{
		std::string account_name;
		AccountDBEntry* entry;
		time_t threshold = ServerInstance->Time() - expiretime;
		time_t* last_used_time;
		std::pair<time_t, bool>* held;
		recentlydropped.clear();
		for (user_hash::const_iterator i = ServerInstance->Users->clientlist->begin(); i != ServerInstance->Users->clientlist->end(); ++i)
		{
			if(!IS_LOCAL(i->second))
				continue;
			account_name = account->GetAccountName(i->second);
			if(account_name.empty())
				continue;
			entry = db->GetAccount(account_name, false);
			if(!entry)
				continue;
			last_used.set(entry, ServerInstance->Time()); // ServerInstance->Time() is inlined, so we would gain nothing by using a temporary variable
			db->SendUpdate(entry, "last_used");
		}
		if(!expiretime) return;
		for (AccountDB::const_iterator i = db->GetDB().begin(); i != db->GetDB().end();)
		{
			entry = i++->second;
			last_used_time = last_used.get(entry);
			held = cmd_hold.held.get(entry);
			if((!last_used_time || *last_used_time < threshold) && !(held && held->second))
			{
				db->RemoveAccount(true, entry);
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
