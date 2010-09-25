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

class ModuleAccountRegister : public Module
{
	std::string hashtype;
	CommandRegister cmd_register;
	CommandChgpass cmd_chgpass;
	CommandFchgpass cmd_fchgpass;
	CommandDrop cmd_drop;
	CommandFdrop cmd_fdrop;

 public:
	ModuleAccountRegister() : cmd_register(this, hashtype), cmd_chgpass(this, hashtype), cmd_fchgpass(this, hashtype), cmd_drop(this), cmd_fdrop(this)
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
	}

	void ReadConfig(ConfigReadStatus&)
	{
		ConfigTag* conf = ServerInstance->Config->GetTag("acctregister");
		hashtype = conf->getString("hashtype", "plaintext");
	}

	void Prioritize()
	{
		// database reading may depend on extension item providers being loaded
		ServerInstance->Modules->SetPriority(this, I_ModuleInit, PRIORITY_AFTER, ServerInstance->Modules->Find("m_account.so"));
	}

	Version GetVersion()
	{
		return Version("Allow users to register their own accounts", VF_VENDOR);
	}
};

MODULE_INIT(ModuleAccountRegister)
