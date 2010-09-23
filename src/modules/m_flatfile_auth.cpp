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

/* $ModDesc: Allow/Deny connections based upon a flat-file database */

static dynamic_reference<AccountProvider> account("account");

/* structure for single entry */
class AccountDBEntry : public Extensible
{
 public:
	irc::string name;
	time_t ts, hash_password_ts, connectclass_ts, tag_ts;
	std::string hash, password, connectclass, tag;
	AccountDBEntry() : Extensible(EXTENSIBLE_ACCOUNT), name(""), ts(0), hash_password_ts(0), connectclass_ts(0), tag_ts(0), hash(""), password(""), connectclass(""), tag("") {}
};

/******************************************************************************
 * Flat-file database read/write
 ******************************************************************************/

typedef std::map<irc::string, AccountDBEntry*> AccountDB;

/** reads the account database and returns structure with it */
class DatabaseReader
{
	/* file */
	FILE *fd;
 public:
	/* constructor, opens the file */
	DatabaseReader (std::string filename)
	{
		/* initialize */
		fd = NULL;
		/* open the file */
		fd = fopen (filename.c_str ( ), "r");
		/* if we can't open the file, return. */
		if (!fd) return;
	}
	/* destructor will close the file */
	~DatabaseReader ( )
	{
		/* if fd is not null, close it */
		if (fd) fclose (fd);
	}
	/* get next entry */
	AccountDBEntry *next ( )
	{
		/* if fd is NULL, fake eof */
		if (!fd) return 0;
		std::string str;
		/* read single characters from the file and add them to the string, end at EOF or \n, ignore \r */
		while (1)
		{
			int c = fgetc (fd);
			if ((c == EOF) || ((unsigned char)c == '\n')) break;
			if ((unsigned char)c == '\r') continue;
			str.push_back ((unsigned char)c);
		}
		/* ready to parse the line */
		if (str == "") return 0;
		AccountDBEntry* entry = new AccountDBEntry();
		std::string token;
		irc::spacesepstream sep(str);
		/* get first one */
		/* malformed if it is not acctinfo */
		if (!sep.GetToken (token) || token != "acctinfo")
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - entry didn't start with acctinfo");
			entry->cull();
			delete entry;
			return 0;
		}
		/* read account name */
		/* if we don't have one, database is malformed */
		if (!sep.GetToken (entry->name))
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - ran out of tokens, expected name");
			entry->cull();
			delete entry;
			return 0;
		}
		/* read account TS */
		/* if we don't have one, database is malformed */
		if (!sep.GetToken (token))
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - ran out of tokens, expected ts");
			entry->cull();
			delete entry;
			return 0;
		}
		entry->ts = atol(token.c_str());
		/* read hash type */
		/* if we don't have one, database is malformed */
		if (!sep.GetToken (entry->hash))
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - ran out of tokens, expected hash");
			entry->cull();
			delete entry;
			return 0;
		}
		/* read password */
		/* if we don't have one, database is malformed */
		if (!sep.GetToken (entry->password))
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - ran out of tokens, expected password");
			entry->cull();
			delete entry;
			return 0;
		}
		/* read hash and password TS */
		/* if we don't have one, database is malformed */
		if (!sep.GetToken (token))
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - ran out of tokens, expected hash/password ts");
			entry->cull();
			delete entry;
			return 0;
		}
		entry->hash_password_ts = atol(token.c_str());
		/* read connect class */
		/* if we don't have one, database is malformed. if it's blank, GetToken returns true and fills in an empty string */
		if (!sep.GetToken (entry->connectclass))
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - ran out of tokens, expected connectclass");
			entry->cull();
			delete entry;
			return 0;
		}
		/* read connect class TS */
		/* if we don't have one, database is malformed */
		if (!sep.GetToken (token))
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - ran out of tokens, expected connectclass ts");
			entry->cull();
			delete entry;
			return 0;
		}
		entry->connectclass_ts = atol(token.c_str());
		/* read tag */
		/* if we don't have one, database is malformed. if it's blank, GetToken returns true and fills in an empty string */
		if (!sep.GetToken (entry->tag))
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - ran out of tokens, expected tag");
			entry->cull();
			delete entry;
			return 0;
		}
		/* read tag TS */
		/* if we don't have one, database is malformed */
		if (!sep.GetToken (token))
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - ran out of tokens, expected tag ts");
			entry->cull();
			delete entry;
			return 0;
		}
		entry->tag_ts = atol(token.c_str());
		/* initial entry read, read next lines in a loop until end, eof means malformed database again */
		while (1)
		{
			str.clear ( );
			/* read the line */
			while (1)
			{
				int c = fgetc (fd);
				if (c == EOF)
				{
					ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - eof, expected end");
					entry->cull();
					delete entry;
					return 0;
				}
					unsigned char c2 = (unsigned char)c;
				if (c2 == '\n') break;
				if (c2 == '\r') continue;
				str.push_back (c2);
			}
			irc::spacesepstream sep2(str);
			/* get the token */
			if (str == "")
			{
				ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - empty line");
				entry->cull();
				delete entry;
				return 0;
			}
			sep2.GetToken (token);
			/* break the loop if token is "end" */
			if (token == "end") break;
			ExtensionItem* ext = ServerInstance->Extensions.GetItem(token);
			if (ext) ext->unserialize(FORMAT_PERSIST, entry, sep2.GetRemaining());
		}
		return entry;
	}
};
/* class being a database writer, gets a database file name on construct */
class DatabaseWriter
{
	/* file stream */
	FILE *fd;
	std::string dbname, tmpname;
	/* public */
	public:
	/* constructor */
	DatabaseWriter (std::string filename)
	{
		fd = NULL;
		dbname = filename;
		tmpname = filename + ".tmp";
		/* ready, open temporary database */
		fd = fopen (tmpname.c_str ( ), "w");
		if (!fd)
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "cannot save to the account database");
		}
	}
	/* destructor */
	~DatabaseWriter ( )
	{
		if (fd)
		{
			/* saving has been ended, close the database and flush buffers */
			fclose (fd);
			/* rename the database file */
			if (rename (tmpname.c_str ( ), dbname.c_str ( )) == -1)
			{
				ServerInstance->Logs->Log ("MODULE", DEFAULT, "can't rename the database file");
			}
		}
	}
	/* save the single entry */
	void next (const AccountDBEntry* ent)
	{
		if (!fd) return;
		/* first, construct the acctinfo line */
		std::string line;
		line.append("acctinfo ").append (ent->name).append (" ").append (ConvToStr(ent->ts)).append (" ")
			.append (ent->hash).append (" ").append (ent->password).append (" ")
			.append (ConvToStr(ent->hash_password_ts)).append (" ").append (ent->connectclass).append (" ")
			.append (ConvToStr(ent->connectclass_ts)).append (" ").append (ent->tag).append (" ")
			.append (ConvToStr(ent->tag_ts)).append ("\n");
		if (fputs (line.c_str ( ), fd) == EOF)
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "unable to write the account entry");
			fclose (fd);
			fd = NULL;
			return;
		}
		for(Extensible::ExtensibleStore::const_iterator i = ent->GetExtList().begin(); i != ent->GetExtList().end(); ++i)
		{
			ExtensionItem* item = i->first;
			std::string value = item->serialize(FORMAT_PERSIST, ent, i->second);
			if (!value.empty())
			{
				std::string toWrite = item->name + " " + value + "\n";
				if (fputs (toWrite.c_str ( ), fd) == EOF)
				{
					ServerInstance->Logs->Log ("MODULE", DEFAULT, "can't write extension item entry");
					fclose (fd);
					fd = NULL;
					return;
				}
			}
		}
		if (fputs ("end\n", fd) == EOF)
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "can't write end of account entry");
			fclose (fd);
			fd = NULL;
			return;
		}
	}
};

/** Handle /ACCTINFO
 */
class CommandAcctinfo : public Command
{
	AccountDB& db;
	bool& dirty;
 public:
	CommandAcctinfo(Module* Creator, AccountDB& db_ref, bool& dirty_ref) : Command(Creator,"ACCTINFO", 3, 6), db(db_ref), dirty(dirty_ref)
	{
		flags_needed = FLAG_SERVERONLY; syntax = "ADD|SET|DEL <account name> <account TS> [key] [value TS] [value]";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		dirty = true;
		AccountDB::iterator iter = db.find(parameters[1]);
		if(parameters[0] == "SET")
		{
			if(iter == db.end()) return CMD_FAILURE; /* if this ever happens, we're desynced */
			if(iter->second->ts < atol(parameters[2].c_str())) return CMD_FAILURE; /* we have an older account with the same name */
			if(iter->second->ts > atol(parameters[2].c_str()))
			{
				/* Nuke the entry. */
				iter->second->cull();
				delete iter->second;
				db.erase(iter);
				AccountDBEntry* entry = new AccountDBEntry();
				entry->name = parameters[1];
				entry->ts = atol(parameters[2].c_str());
				std::pair<AccountDB::iterator, bool> result = db.insert(std::make_pair(parameters[1], entry));
				iter = result.first;
			}
			ExtensionItem* ext = ServerInstance->Extensions.GetItem(parameters[3]);
			if (ext)
			{
				if(parameters.size() > 5) ext->unserialize(FORMAT_NETWORK, iter->second, parameters[4] + " " + parameters[5]);
				else ext->unserialize(FORMAT_NETWORK, iter->second, parameters[4]);
			}
			if(parameters[3] == "hash_password")
			{
				if(iter->second->hash_password_ts > atol(parameters[4].c_str())) return CMD_FAILURE;
				size_t delim;
				if(parameters.size() > 5 && (delim = parameters[5].find_first_of(' ')) != std::string::npos)
				{
					iter->second->hash = parameters[5].substr(0, delim);
					iter->second->password = parameters[5].substr(delim + 1);
				}
				else iter->second->hash = iter->second->password = "";
				iter->second->hash_password_ts = atol(parameters[4].c_str());
			}
			else if(parameters[3] == "connectclass")
			{
				if(iter->second->connectclass_ts > atol(parameters[4].c_str())) return CMD_FAILURE;
				iter->second->connectclass = parameters.size() > 5 ? parameters[5] : "";
				iter->second->connectclass_ts = atol(parameters[4].c_str());
			}
			else if(parameters[3] == "tag")
			{
				if(iter->second->tag_ts > atol(parameters[4].c_str())) return CMD_FAILURE;
				iter->second->tag = parameters.size() > 5 ? parameters[5] : "";
				iter->second->tag_ts = atol(parameters[4].c_str());
			}
		}
		else if(parameters[0] == "ADD")
		{
			if(iter == db.end() || iter->second->ts > atol(parameters[2].c_str()))
			{
				if(iter->second->ts > atol(parameters[2].c_str()))
				{
					iter->second->cull();
					delete iter->second;
					db.erase(iter);
				}
				AccountDBEntry* entry = new AccountDBEntry();
				entry->name = parameters[1];
				entry->ts = atol(parameters[2].c_str());
				std::pair<AccountDB::iterator, bool> result = db.insert(std::make_pair(parameters[1], entry));
				iter = result.first;
			}
			else if(iter->second->ts < atol(parameters[2].c_str())) return CMD_FAILURE;
		}
		else if(parameters[0] == "DEL")
		{
			if(iter != db.end())
			{
				if(iter->second->ts < atol(parameters[2].c_str())) return CMD_FAILURE;
				db.erase(iter);
			}
		}
		else return CMD_FAILURE;
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

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		irc::string username;
		std::string password;
		if(parameters.size() == 1)
		{
			username = user->nick;
			password = parameters[0];
		}
		else
		{
			username = parameters[0];
			password = parameters[1];
		}
		AccountDB::iterator iter = db.find(username);
		if(iter == db.end() || iter->second->password.empty() || ServerInstance->PassCompare(user, iter->second->password, password, iter->second->hash))
		{
			user->WriteServ("NOTICE " + user->nick + " :Invalid username or password");
			return CMD_FAILURE;
		}
		if(account) account->DoLogin(user, iter->first, iter->second->tag);
		if(!iter->second->connectclass.empty()) ServerInstance->ForcedClass.set(user, iter->second->connectclass);
		return CMD_SUCCESS;
	}
};

class ModuleFlatfileAuth : public Module
{
 private:
	std::string dbfile;
	bool dirty; // dbfile needs to be flushed to disk

	AccountDB db;
	CommandAcctinfo cmd_acctinfo;
	CommandLogin cmd_login;

	void WriteFileDatabase ( )
	{
		// Dump entire database; open/close in constructor/destructor
		DatabaseWriter dbwriter (dbfile);
		for (AccountDB::const_iterator i = db.begin(); i != db.end(); ++i)
			dbwriter.next(i->second);
	}

	/** Read flat-file database */
	void ReadFileDatabase ( )
	{
		/* create the reader object and open the database */
		DatabaseReader dbreader (dbfile);
		/* start the database read loop */
		AccountDBEntry *entry;
		while ((entry = dbreader.next ( )))
		{
			std::pair<AccountDB::iterator, bool> result = db.insert(std::make_pair(entry->name, entry));
			/* if it didn't insert because we already have one, look at its TS */
			if(!result.second)
			{
				AccountDBEntry* existing = result.first->second;
				/* if the TS's are the same, merge each field individually */
				if(entry->ts == existing->ts)
				{
					if(entry->hash_password_ts > existing->hash_password_ts)
					{
						existing->hash = entry->hash;
						existing->password = entry->password;
						existing->hash_password_ts = entry->hash_password_ts;
					}
					if(entry->connectclass_ts > existing->connectclass_ts)
					{
						existing->connectclass = entry->connectclass;
						existing->connectclass_ts = entry->connectclass_ts;
					}
					if(entry->tag_ts > existing->tag_ts)
					{
						existing->tag = entry->tag;
						existing->tag_ts = entry->tag_ts;
					}
					for(Extensible::ExtensibleStore::const_iterator it = entry->GetExtList().begin(); it != entry->GetExtList().end(); ++it)
					{
						ExtensionItem* item = it->first;
						std::string value = item->serialize(FORMAT_INTERNAL, entry, it->second);
						if(!value.empty()) item->unserialize(FORMAT_INTERNAL, existing, value);
					}
				}
				/* if this one is older, replace the existing one with it completely */
				else if(entry->ts < existing->ts)
				{
					result.first->second->cull();
					delete result.first->second;
					db.erase(result.first);
					db.insert(std::make_pair(entry->name, entry));
				}
				/* the third case is that the one we read is newer, in which case we get rid of it */
				else
				{
					entry->cull();
					delete entry;
					continue;
				}
			}
			std::vector<std::string> params;
			params.push_back("*");
			params.push_back("ACCTINFO");
			params.push_back("ADD");
			params.push_back(entry->name);
			params.push_back(":" + ConvToStr(entry->ts));
			ServerInstance->PI->SendEncapsulatedData(params);
			params.clear();
			params.push_back("*");
			params.push_back("ACCTINFO");
			params.push_back("SET");
			params.push_back(entry->name);
			params.push_back(ConvToStr(entry->ts));
			params.push_back("hash_password");
			params.push_back(ConvToStr(entry->hash_password_ts));
			params.push_back(":" + entry->hash + " " + entry->password);
			ServerInstance->PI->SendEncapsulatedData(params);
			params.clear();
			params.push_back("*");
			params.push_back("ACCTINFO");
			params.push_back("SET");
			params.push_back(entry->name);
			params.push_back(ConvToStr(entry->ts));
			params.push_back("connectclass");
			params.push_back(ConvToStr(entry->connectclass_ts));
			params.push_back(":" + entry->connectclass);
			ServerInstance->PI->SendEncapsulatedData(params);
			params.clear();
			params.push_back("*");
			params.push_back("ACCTINFO");
			params.push_back("SET");
			params.push_back(entry->name);
			params.push_back(ConvToStr(entry->ts));
			params.push_back("tag");
			params.push_back(ConvToStr(entry->tag_ts));
			params.push_back(":" + entry->tag);
			ServerInstance->PI->SendEncapsulatedData(params);
			for(Extensible::ExtensibleStore::const_iterator it = entry->GetExtList().begin(); it != entry->GetExtList().end(); ++it)
			{
				ExtensionItem* item = it->first;
				std::string value = item->serialize(FORMAT_NETWORK, entry, it->second);
				if (!value.empty())
				{
					params.clear();
					params.push_back("*");
					params.push_back("ACCTINFO");
					params.push_back("SET");
					params.push_back(entry->name);
					params.push_back(ConvToStr(entry->ts));
					params.push_back(item->name);
					params.push_back(value);
					ServerInstance->PI->SendEncapsulatedData(params);
				}
			}
		}
	}

 public:
	ModuleFlatfileAuth() : dirty(true), cmd_acctinfo(this, db, dirty), cmd_login(this, db)
	{
	}

	void init()
	{
		ServerInstance->AddCommand(&cmd_acctinfo);
		ServerInstance->AddCommand(&cmd_login);
		Implementation eventlist[] = { I_OnSyncNetwork, I_OnBackgroundTimer, I_OnUnloadModule };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		ReadFileDatabase();
	}

	virtual ~ModuleFlatfileAuth()
	{
		for(AccountDB::iterator iter = db.begin(); iter != db.end(); ++iter)
		{
			iter->second->cull();
			delete iter->second;
		}
		db.clear();
	}

	void ReadConfig(ConfigReadStatus&)
	{
		ConfigTag* conf = ServerInstance->Config->GetTag("flatfileauth");
		dbfile = conf->getString("dbfile");
	}

	void OnSyncNetwork(SyncTarget* target)
	{
		for (AccountDB::const_iterator i = db.begin(); i != db.end(); ++i)
		{
			std::string name = i->first, ts = ConvToStr(i->second->ts);
			target->SendCommand("ENCAP * ACCTINFO ADD " + name + " :" + ts);
			target->SendCommand("ENCAP * ACCTINFO SET " + name + " " + ts + " hash_password "
				+ ConvToStr(i->second->hash_password_ts) + " :" + i->second->hash + " " + i->second->password);
			target->SendCommand("ENCAP * ACCTINFO SET " + name + " " + ts + " connectclass "
				+ ConvToStr(i->second->connectclass_ts) + " :" + i->second->connectclass);
			target->SendCommand("ENCAP * ACCTINFO SET " + name + " " + ts + " tag "
				+ ConvToStr(i->second->tag_ts) + " :" + i->second->tag);
			for(Extensible::ExtensibleStore::const_iterator it = i->second->GetExtList().begin(); it != i->second->GetExtList().end(); ++it)
			{
				ExtensionItem* item = it->first;
				std::string value = item->serialize(FORMAT_NETWORK, i->second, it->second);
				if (!value.empty())
					target->SendCommand("ENCAP * ACCTINFO SET " + name + " " + ts + " " + item->name + " " + value);
			}
		}
	}

	/* called on background timer to write all accounts to disk if they were changed */
	void OnBackgroundTimer (time_t cur)
	{
		/* if not dirty then don't do anything */
		if (!dirty)
			return;
		/* dirty, an account was changed, save it */
		WriteFileDatabase();
		/* clear dirty to prevent next savings */
		dirty = false;
	}

	void OnUnloadModule(Module* mod)
	{
		std::vector<reference<ExtensionItem> > acct_exts;
		ServerInstance->Extensions.BeginUnregister(mod, EXTENSIBLE_ACCOUNT, acct_exts);
		for(AccountDB::iterator iter = db.begin(); iter != db.end(); ++iter)
		{
			mod->OnCleanup(TYPE_OTHER, iter->second);
			iter->second->doUnhookExtensions(acct_exts);
		}
	}

	Version GetVersion()
	{
		return Version("Allow/Deny connections based upon a flatfile database", VF_VENDOR);
	}
};

MODULE_INIT(ModuleFlatfileAuth)
