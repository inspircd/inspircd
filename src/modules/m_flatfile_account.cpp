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

/* $ModDesc: Read and write accounts from a flat-file database */

static dynamic_reference<AccountProvider> account("account");
static dynamic_reference<AccountDBProvider> db("accountdb");

/******************************************************************************
 * Flat-file database read/write
 ******************************************************************************/

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
	/* read and store next entry */
	bool next ( )
	{
		/* if fd is NULL, fake eof */
		if (!fd) return false;
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
		if (str == "") return false;
		irc::string name;
		time_t ts, hash_password_ts;
		std::string hash, password;
		std::map<std::string, std::string> extensions;
		std::string token;
		irc::spacesepstream sep(str);
		/* get first one */
		/* malformed if it is not acctinfo */
		if (!sep.GetToken (token) || token != "acctinfo")
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - entry didn't start with acctinfo");
			return false;
		}
		/* read account name */
		/* if we don't have one, database is malformed */
		if (!sep.GetToken (name))
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - ran out of tokens, expected name");
			return false;
		}
		/* read account TS */
		/* if we don't have one, database is malformed */
		if (!sep.GetToken (token))
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - ran out of tokens, expected ts");
			return false;
		}
		ts = atol(token.c_str());
		/* read hash type */
		/* if we don't have one, database is malformed */
		if (!sep.GetToken (hash))
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - ran out of tokens, expected hash");
			return false;
		}
		/* read password */
		/* if we don't have one, database is malformed */
		if (!sep.GetToken (password))
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - ran out of tokens, expected password");
			return false;
		}
		/* read hash and password TS */
		/* if we don't have one, database is malformed */
		if (!sep.GetToken (token))
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - ran out of tokens, expected hash/password ts");
			return false;
		}
		hash_password_ts = atol(token.c_str());
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
					return false;
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
				return false;
			}
			sep2.GetToken (token);
			/* break the loop if token is "end" */
			if (token == "end") break;
			extensions.insert(std::make_pair(token, sep2.GetRemaining()));
		}
		AccountDBEntry* entry = db->GetAccount(name, false);
		if(!entry || entry->ts > ts)
		{
			if(entry)
				db->RemoveAccount(false, entry);
			entry = db->AddAccount(false, name, ts, hash, password, hash_password_ts);
		}
		else if(entry->ts == ts)
		{
			if(hash_password_ts > entry->hash_password_ts)
			{
				entry->hash = hash;
				entry->password = password;
				entry->hash_password_ts = hash_password_ts;
			}
		}
		else
			return true;
		for(std::map<std::string, std::string>::iterator i = extensions.begin(); i != extensions.end(); ++i)
		{
			ExtensionItem* ext = ServerInstance->Extensions.GetItem(i->first);
			if (ext) ext->unserialize(FORMAT_PERSIST, entry, i->second);
		}
		db->SendAccount(entry);
		return true;
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
			.append (ConvToStr(ent->hash_password_ts)).append ("\n");
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

class ModuleFlatfileAccount : public Module
{
 private:
	std::string dbfile;
	bool dirty; // dbfile needs to be flushed to disk

	void WriteFileDatabase ( )
	{
		// Dump entire database; open/close in constructor/destructor
		DatabaseWriter dbwriter (dbfile);
		for (AccountDB::const_iterator i = db->GetDB().begin(); i != db->GetDB().end(); ++i)
			dbwriter.next(i->second);
	}

 public:
	ModuleFlatfileAccount() : dirty(true)
	{
	}

	void init()
	{
		if(!db) throw ModuleException("m_flatfile_account requires that m_account be loaded");
		Implementation eventlist[] = { I_OnBackgroundTimer, I_OnEvent };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		for(DatabaseReader dbreader(dbfile);dbreader.next(););
	}

	void ReadConfig(ConfigReadStatus&)
	{
		ConfigTag* conf = ServerInstance->Config->GetTag("flatfileacct");
		dbfile = conf->getString("dbfile");
	}

	/* called on background timer to write all accounts to disk if they were changed */
	void OnBackgroundTimer (time_t cur)
	{
		/* if not dirty then don't do anything */
		if (!dirty)
			return;
		/* dirty, an account was changed, save it */
		if(db) WriteFileDatabase();
		/* clear dirty to prevent next savings */
		dirty = false;
	}

	void OnEvent(Event& event)
	{
		if(event.id == "accountdb_modified") dirty = true;
	}

	void Prioritize()
	{
		// database reading may depend on extension item providers being loaded
		ServerInstance->Modules->SetPriority(this, I_ModuleInit, PRIORITY_LAST);
	}

	Version GetVersion()
	{
		return Version("Read and write accounts from a flat-file database", VF_VENDOR);
	}
};

MODULE_INIT(ModuleFlatfileAccount)
