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
	/* get next entry */
	AccountDBEntry *next ( )
	{
		/* if fd is NULL, fake eof */
		if (!fd) return NULL;
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
		if (str == "") return NULL;
		irc::string name;
		std::string token;
		irc::spacesepstream sep(str);
		/* get first one */
		/* malformed if it is not acctinfo */
		if (!sep.GetToken (token) || token != "acctinfo")
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - entry didn't start with acctinfo");
			return NULL;
		}
		/* read account name */
		/* if we don't have one, database is malformed */
		if (!sep.GetToken (name))
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - ran out of tokens, expected name");
			return NULL;
		}
		/* read account TS */
		/* if we don't have one, database is malformed */
		if (!sep.GetToken (token))
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - ran out of tokens, expected ts");
			return NULL;
		}
		AccountDBEntry* entry = new AccountDBEntry(name, atol(token.c_str()));
		/* read hash type */
		/* if we don't have one, database is malformed */
		if (!sep.GetToken (entry->hash))
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - ran out of tokens, expected hash");
			entry->cull();
			delete entry;
			return NULL;
		}
		/* read password */
		/* if we don't have one, database is malformed */
		if (!sep.GetToken (entry->password))
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - ran out of tokens, expected password");
			entry->cull();
			delete entry;
			return NULL;
		}
		/* read hash and password TS */
		/* if we don't have one, database is malformed */
		if (!sep.GetToken (token))
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - ran out of tokens, expected hash/password ts");
			entry->cull();
			delete entry;
			return NULL;
		}
		entry->hash_password_ts = atol(token.c_str());
		/* read connect class */
		/* if we don't have one, database is malformed. if it's blank, GetToken returns true and fills in an empty string */
		if (!sep.GetToken (entry->connectclass))
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - ran out of tokens, expected connectclass");
			entry->cull();
			delete entry;
			return NULL;
		}
		/* read connect class TS */
		/* if we don't have one, database is malformed */
		if (!sep.GetToken (token))
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - ran out of tokens, expected connectclass ts");
			entry->cull();
			delete entry;
			return NULL;
		}
		entry->connectclass_ts = atol(token.c_str());
		/* read tag */
		/* if we don't have one, database is malformed. if it's blank, GetToken returns true and fills in an empty string */
		if (!sep.GetToken (entry->tag))
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - ran out of tokens, expected tag");
			entry->cull();
			delete entry;
			return NULL;
		}
		/* read tag TS */
		/* if we don't have one, database is malformed */
		if (!sep.GetToken (token))
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed account database - ran out of tokens, expected tag ts");
			entry->cull();
			delete entry;
			return NULL;
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
					return NULL;
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
				return NULL;
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

	/** Read flat-file database */
	void ReadFileDatabase ( )
	{
		/* create the reader object and open the database */
		DatabaseReader dbreader (dbfile);
		/* start the database read loop */
		AccountDBEntry *entry;
		while ((entry = dbreader.next ( )))
		{
			AccountDBEntry* existing = db->GetAccount(entry->name);
			if(!existing)
				db->AddAccount(entry, true);
			else
			{
				/* if the TS's are the same, merge each field individually */
				if(entry->ts == existing->ts)
				{
					if(entry->hash_password_ts > existing->hash_password_ts)
					{
						existing->hash = entry->hash;
						existing->password = entry->password;
						existing->hash_password_ts = entry->hash_password_ts;
						db->SendUpdate(existing, "hash_password");
					}
					if(entry->connectclass_ts > existing->connectclass_ts)
					{
						existing->connectclass = entry->connectclass;
						existing->connectclass_ts = entry->connectclass_ts;
						db->SendUpdate(existing, "connectclass");
					}
					if(entry->tag_ts > existing->tag_ts)
					{
						existing->tag = entry->tag;
						existing->tag_ts = entry->tag_ts;
						db->SendUpdate(existing, "tag");
					}
					for(Extensible::ExtensibleStore::const_iterator it = entry->GetExtList().begin(); it != entry->GetExtList().end(); ++it)
					{
						ExtensionItem* item = it->first;
						std::string value = item->serialize(FORMAT_INTERNAL, entry, it->second);
						if(!value.empty())
						{
							item->unserialize(FORMAT_INTERNAL, existing, value);
							db->SendUpdate(existing, item->name);
						}
					}
				}
				/* if this one is older, replace the existing one with it completely */
				else if(entry->ts < existing->ts)
				{
					db->RemoveAccount(existing, false);
					existing->cull();
					delete existing;
					db->AddAccount(entry, true);
				}
				/* the third case is that the one we read is newer, in which case we get rid of it */
				else
				{
					entry->cull();
					delete entry;
				}
			}
		}
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
		ReadFileDatabase();
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
