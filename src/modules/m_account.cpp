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

/* $ModDesc: Store account information and allow logging in to them ircd-side */

static dynamic_reference<AccountProvider> account("account");

class AccountDBEntryImpl : public AccountDBEntry
{
 public:
	AccountDBEntryImpl(const irc::string& nameref, time_t ourTS, std::string h = "", std::string p = "", time_t h_p_ts = 0, std::string cc = "", time_t cc_ts = 0) : AccountDBEntry(nameref, ourTS, h, p, h_p_ts, cc, cc_ts)
	{
	}
	virtual CullResult cull()
	{
		return Extensible::cull();
	}
	virtual ~AccountDBEntryImpl()
	{
	}
};

class AccountDBProviderImpl : public AccountDBProvider
{
 public:
	AccountDB db;

	AccountDBProviderImpl(Module* parent) : AccountDBProvider(parent) {}

	AccountDBEntry* AddAccount(bool send, const irc::string& nameref, time_t ourTS, std::string h = "", std::string p = "", time_t h_p_ts = 0, std::string cc = "", time_t cc_ts = 0)
	{
		if(db.find(nameref) != db.end())
			return NULL;
		AccountDBEntry* entry = new AccountDBEntryImpl(nameref, ourTS, h, p, h_p_ts, cc, cc_ts);
		db.insert(std::make_pair(nameref, entry));
		if(send)
			SendAccount(entry);
		return entry;
	}

	AccountDBEntry* GetAccount(irc::string acctname) const
	{
		AccountDB::const_iterator res = db.find(acctname);
		return res != db.end() ? res->second : NULL;
	}

	void RemoveAccount(bool send, AccountDBEntry* entry)
	{
		db.erase(entry->name);
		if(account)
			for (user_hash::const_iterator i = ServerInstance->Users->clientlist->begin(); i != ServerInstance->Users->clientlist->end(); ++i)
			{
				if(!IS_LOCAL(i->second))
					continue;
				if(entry->name == account->GetAccountName(i->second))
				{
					account->DoLogin(i->second, "", "");
					i->second->WriteServ("NOTICE " + i->second->nick + " :Account " + std::string(entry->name) + " has been dropped");
				}
			}
		if(send)
			SendRemoval(entry->name, entry->ts);
		entry->cull();
		delete entry;
	}

	const AccountDB& GetDB() const
	{
		return db;
	}

	void SendAccount(const AccountDBEntry* entry) const
	{
		std::vector<std::string> params;
		params.push_back("*");
		params.push_back("SVSACCOUNT");
		params.push_back("ADD");
		params.push_back(entry->name);
		params.push_back(":" + ConvToStr(entry->ts));
		ServerInstance->PI->SendEncapsulatedData(params);
		params.clear();
		params.push_back("*");
		params.push_back("SVSACCOUNT");
		params.push_back("SET");
		params.push_back(entry->name);
		params.push_back(ConvToStr(entry->ts));
		params.push_back("hash_password");
		params.push_back(ConvToStr(entry->hash_password_ts));
		params.push_back(":" + entry->hash + " " + entry->password);
		ServerInstance->PI->SendEncapsulatedData(params);
		params.clear();
		params.push_back("*");
		params.push_back("SVSACCOUNT");
		params.push_back("SET");
		params.push_back(entry->name);
		params.push_back(ConvToStr(entry->ts));
		params.push_back("connectclass");
		params.push_back(ConvToStr(entry->connectclass_ts));
		params.push_back(":" + entry->connectclass);
		ServerInstance->PI->SendEncapsulatedData(params);
		for(Extensible::ExtensibleStore::const_iterator it = entry->GetExtList().begin(); it != entry->GetExtList().end(); ++it)
		{
			ExtensionItem* item = it->first;
			std::string value = item->serialize(FORMAT_NETWORK, entry, it->second);
			if (!value.empty())
			{
				params.clear();
				params.push_back("*");
				params.push_back("SVSACCOUNT");
				params.push_back("SET");
				params.push_back(entry->name);
				params.push_back(ConvToStr(entry->ts));
				params.push_back(item->name);
				params.push_back(value);
				ServerInstance->PI->SendEncapsulatedData(params);
			}
		}
		AccountDBModifiedEvent(creator, entry->name, entry).Send();
	}

	void SendUpdate(const AccountDBEntry* entry, std::string field) const
	{
		std::vector<std::string> params;
		params.push_back("*");
		params.push_back("SVSACCOUNT");
		params.push_back("SET");
		params.push_back(entry->name);
		params.push_back(ConvToStr(entry->ts));
		params.push_back(field);
		if(field == "hash_password")
		{
			params.push_back(ConvToStr(entry->hash_password_ts));
			params.push_back(":" + entry->hash + " " + entry->password);
		}
		else if(field == "connectclass")
		{
			params.push_back(ConvToStr(entry->connectclass_ts));
			params.push_back(":" + entry->connectclass);
		}
		else
		{
			ExtensionItem* ext = ServerInstance->Extensions.GetItem(field);
			if(!ext)
				return;
			Extensible::ExtensibleStore::const_iterator it = entry->GetExtList().find(ext);
			if(it == entry->GetExtList().end())
				return;
			std::string value = ext->serialize(FORMAT_NETWORK, entry, it->second);
			if(value.empty())
				return;
			params.push_back(value);
		}
		ServerInstance->PI->SendEncapsulatedData(params);
		AccountDBModifiedEvent(creator, entry->name, entry).Send();
	}

	void SendRemoval(irc::string acctname, time_t ts) const
	{
		std::vector<std::string> params;
		params.push_back("*");
		params.push_back("SVSACCOUNT");
		params.push_back("DEL");
		params.push_back(acctname);
		params.push_back(ConvToStr(ts));
		ServerInstance->PI->SendEncapsulatedData(params);
		AccountDBModifiedEvent(creator, acctname, NULL).Send();
	}
};

/** Handle /SVSACCOUNT
 */
class CommandSvsaccount : public Command
{
 public:
	AccountDBProviderImpl prov;
	CommandSvsaccount(Module* Creator) : Command(Creator,"SVSACCOUNT", 3, 6), prov(Creator)
	{
		flags_needed = FLAG_SERVERONLY; syntax = "ADD|SET|DEL <account name> <account TS> [key] [value TS] [value]";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		AccountDB::iterator iter = prov.db.find(parameters[1]);
		if(parameters[0] == "SET")
		{
			if(parameters.size() < 5)
				return CMD_INVALID; /* this form of the command needs at least 5 parameters */
			if(iter == prov.db.end())
				return CMD_FAILURE; /* if this ever happens, we're desynced */
			if(iter->second->ts < atol(parameters[2].c_str()))
				return CMD_FAILURE; /* we have an older account with the same name */
			if(iter->second->ts > atol(parameters[2].c_str()))
			{
				/* Nuke the entry. */
				prov.RemoveAccount(false, iter->second);
				AccountDBEntry* entry = new AccountDBEntryImpl(parameters[1], atol(parameters[2].c_str()));
				iter = prov.db.insert(std::make_pair(parameters[1], entry)).first;
			}
			ExtensionItem* ext = ServerInstance->Extensions.GetItem(parameters[3]);
			if (ext)
			{
				if(parameters.size() > 5)
					ext->unserialize(FORMAT_NETWORK, iter->second, parameters[4] + " " + parameters[5]);
				else
					ext->unserialize(FORMAT_NETWORK, iter->second, parameters[4]);
			}
			if(parameters[3] == "hash_password")
			{
				if(iter->second->hash_password_ts > atol(parameters[4].c_str()))
					return CMD_FAILURE;
				std::string::size_type delim;
				if(parameters.size() > 5 && (delim = parameters[5].find_first_of(' ')) != std::string::npos)
				{
					iter->second->hash = parameters[5].substr(0, delim);
					iter->second->password = parameters[5].substr(delim + 1);
				}
				else
					iter->second->hash = iter->second->password = "";
				iter->second->hash_password_ts = atol(parameters[4].c_str());
			}
			else if(parameters[3] == "connectclass")
			{
				if(iter->second->connectclass_ts > atol(parameters[4].c_str()))
					return CMD_FAILURE;
				iter->second->connectclass = parameters.size() > 5 ? parameters[5] : "";
				iter->second->connectclass_ts = atol(parameters[4].c_str());
			}
			AccountDBModifiedEvent(creator, iter->second->name, iter->second).Send();
		}
		else if(parameters[0] == "ADD")
		{
			if(iter == prov.db.end() || iter->second->ts > atol(parameters[2].c_str()))
			{
				if(iter != prov.db.end())
					prov.RemoveAccount(false, iter->second);
				AccountDBEntry* entry = new AccountDBEntryImpl(parameters[1], atol(parameters[2].c_str()));
				iter = prov.db.insert(std::make_pair(parameters[1], entry)).first;
			}
			else if(iter->second->ts < atol(parameters[2].c_str()))
				return CMD_FAILURE;
			AccountDBModifiedEvent(creator, iter->second->name, iter->second).Send();
		}
		else if(parameters[0] == "DEL")
		{
			if(iter != prov.db.end())
			{
				if(iter->second->ts < atol(parameters[2].c_str()))
					return CMD_FAILURE;
				prov.RemoveAccount(false, iter->second);
				AccountDBModifiedEvent(creator, parameters[1], NULL).Send();
			}
		}
		else
			return CMD_FAILURE;
		return CMD_SUCCESS;
	}
};

/** Handle /LOGIN
 */
class CommandLogin : public Command
{
	AccountDB& db;
 public:
	CommandLogin(Module* Creator, AccountDB& db_ref) : Command(Creator,"LOGIN", 1, 2), db(db_ref)
	{
		syntax = "[account name] <password>";
	}

	bool TryLogin (User* user, irc::string username, std::string password)
	{
		AccountDB::const_iterator iter = db.find(username);
		if(iter == db.end() || iter->second->password.empty() || ServerInstance->PassCompare(user, iter->second->password, password, iter->second->hash))
			return false;
		if(account)
			account->DoLogin(user, iter->first, "");
		if(!iter->second->connectclass.empty())
			ServerInstance->ForcedClass.set(user, iter->second->connectclass);
		return true;
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		bool result;
		if(parameters.size() == 1)
			result = TryLogin(user, user->nick, parameters[0]);
		else
			result = TryLogin(user, parameters[0], parameters[1]);

		if(!result)
		{
			user->WriteServ("NOTICE " + user->nick + " :Invalid username or password");
			return CMD_FAILURE;
		}
		return CMD_SUCCESS;

	}
};

/** Handle /LOGOUT
 */
class CommandLogout : public Command
{
 public:
	CommandLogout(Module* Creator) : Command(Creator,"LOGOUT", 0, 0)
	{
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		if(account)
		{
			if(account->IsRegistered(user))
			{
				account->DoLogin(user, "", "");
				user->WriteServ("NOTICE " + user->nick + " :Logout successful");
			}
			else
				user->WriteServ("NOTICE " + user->nick + " :You are not logged in");
		}
		return CMD_SUCCESS;
	}
};

/** Handle /ACCTLIST
 */
class CommandAcctlist : public Command
{
	AccountDB& db;
 public:
	CommandAcctlist(Module* Creator, AccountDB& db_ref) : Command(Creator,"ACCTLIST", 0, 1), db(db_ref)
	{
		flags_needed = 'o'; syntax = "[pattern]";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		// XXX: Use numerics instead of NOTICEs?
		bool displayAll = parameters.empty() || parameters[0] == "*";
		for(AccountDB::const_iterator iter = db.begin(); iter != db.end(); ++iter)
			if(displayAll || InspIRCd::Match(iter->second->name, parameters[0]))
				user->WriteServ("NOTICE " + user->nick + " :" + std::string(iter->second->name));
		user->WriteServ("NOTICE " + user->nick + " :End of account list");
		return CMD_SUCCESS;
	}
};

/** Handle /ACCTSHOW
 */
class CommandAcctshow : public Command
{
	AccountDB& db;
 public:
	CommandAcctshow(Module* Creator, AccountDB& db_ref) : Command(Creator,"ACCTSHOW", 1, 1), db(db_ref)
	{
		flags_needed = 'o'; syntax = "<account name>";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		AccountDB::const_iterator iter = db.find(parameters[0]);
		if(iter == db.end())
		{
			user->WriteServ("NOTICE " + user->nick + " :No such account");
			return CMD_FAILURE;
		}
		AccountDBEntry* entry = iter->second;
		user->WriteServ("NOTICE " + user->nick + " :Account: \"" + std::string(entry->name) + "\" TS: " +
			ConvToStr(entry->ts) + " Hash type: \"" + entry->hash + "\" Hash/Password TS: " +
			ConvToStr(entry->hash_password_ts) + " Connect class: \"" + entry->connectclass + "\" Connect class TS: " +
			ConvToStr(entry->connectclass_ts));
		for(Extensible::ExtensibleStore::const_iterator it = entry->GetExtList().begin(); it != entry->GetExtList().end(); ++it)
		{
			std::string value = it->first->serialize(FORMAT_USER, entry, it->second);
			if (!value.empty())
				user->WriteServ("NOTICE " + user->nick + " :" + it->first->name + ": " + value);
		}
		return CMD_SUCCESS;
	}
};

class ModuleAccount : public Module
{
 private:
	CommandSvsaccount cmd;
	CommandLogin cmd_login;
	CommandLogout cmd_logout;
	CommandAcctlist cmd_acctlist;
	CommandAcctshow cmd_acctshow;

 public:
	ModuleAccount() : cmd(this), cmd_login(this, cmd.prov.db), cmd_logout(this), cmd_acctlist(this, cmd.prov.db), cmd_acctshow(this, cmd.prov.db)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
		ServerInstance->Modules->AddService(cmd.prov);
		ServerInstance->Modules->AddService(cmd_login);
		ServerInstance->Modules->AddService(cmd_logout);
		ServerInstance->Modules->AddService(cmd_acctlist);
		ServerInstance->Modules->AddService(cmd_acctshow);
		Implementation eventlist[] = { I_OnUserRegister, I_OnSyncNetwork, I_OnUnloadModule };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual ~ModuleAccount()
	{
		for(AccountDB::iterator iter = cmd.prov.db.begin(); iter != cmd.prov.db.end(); ++iter)
		{
			iter->second->cull();
			delete iter->second;
		}
		cmd.prov.db.clear();
	}

	void OnUserRegister(LocalUser* user)
	{
		if (account && account->IsRegistered(user))
			return;
		std::string::size_type sep = user->password.find_first_of(':');
		if(sep != std::string::npos &&
			cmd_login.TryLogin(user, user->password.substr(0, sep), user->password.substr(sep + 1)))
			return;
		if(cmd_login.TryLogin(user, user->nick, user->password))
			return;
		cmd_login.TryLogin(user, user->ident, user->password);
	}

	void OnSyncNetwork(SyncTarget* target)
	{
		for (AccountDB::const_iterator i = cmd.prov.db.begin(); i != cmd.prov.db.end(); ++i)
		{
			std::string name = i->first, ts = ConvToStr(i->second->ts);
			target->SendCommand("ENCAP * SVSACCOUNT ADD " + name + " :" + ts);
			target->SendCommand("ENCAP * SVSACCOUNT SET " + name + " " + ts + " hash_password "
				+ ConvToStr(i->second->hash_password_ts) + " :" + i->second->hash + " " + i->second->password);
			target->SendCommand("ENCAP * SVSACCOUNT SET " + name + " " + ts + " connectclass "
				+ ConvToStr(i->second->connectclass_ts) + " :" + i->second->connectclass);
			for(Extensible::ExtensibleStore::const_iterator it = i->second->GetExtList().begin(); it != i->second->GetExtList().end(); ++it)
			{
				ExtensionItem* item = it->first;
				std::string value = item->serialize(FORMAT_NETWORK, i->second, it->second);
				if (!value.empty())
					target->SendCommand("ENCAP * SVSACCOUNT SET " + name + " " + ts + " " + item->name + " " + value);
			}
		}
	}

	void OnUnloadModule(Module* mod)
	{
		std::vector<reference<ExtensionItem> > acct_exts;
		ServerInstance->Extensions.BeginUnregister(mod, EXTENSIBLE_ACCOUNT, acct_exts);
		for(AccountDB::const_iterator iter = cmd.prov.db.begin(); iter != cmd.prov.db.end(); ++iter)
		{
			mod->OnCleanup(TYPE_OTHER, iter->second);
			iter->second->doUnhookExtensions(acct_exts);
			AccountDBModifiedEvent(this, iter->second->name, iter->second).Send();
		}
	}

	Version GetVersion()
	{
		return Version("Store account information and allow logging in to them ircd-side", VF_VENDOR|VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleAccount)
