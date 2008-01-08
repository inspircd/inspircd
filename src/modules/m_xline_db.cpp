/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "xline.h"

/* $ModDesc: Keeps a dynamic log of all XLines created, and stores them in a seperate conf file (xline.db). */

class ModuleXLineDB : public Module
{
	std::vector<XLine *> xlines;
	bool reading_db;			// If this is true, addlines are as a result of db reading, so don't bother flushing the db to disk.
						// DO REMEMBER TO SET IT, otherwise it's annoying :P
 public:
	ModuleXLineDB(InspIRCd* Me) : Module(Me)
	{
		Implementation eventlist[] = { I_OnAddLine, I_OnDelLine };
		ServerInstance->Modules->Attach(eventlist, this, 2);

		reading_db = true;
		ReadDatabase();
		reading_db = false;
	}

	virtual ~ModuleXLineDB()
	{
	}

	/** Called whenever an xline is added by a local user.
	 * This method is triggered after the line is added.
	 * @param source The sender of the line or NULL for local server
	 * @param line The xline being added
	 */
	void OnAddLine(User* source, XLine* line)
	{
		xlines.push_back(line);

		for (std::vector<XLine *>::iterator i = xlines.begin(); i != xlines.end(); i++)
		{
			line = (*i);
			ServerInstance->WriteOpers("%s %s %s %lu %lu :%s", line->type.c_str(), line->Displayable(),
ServerInstance->Config->ServerName, line->set_time, line->duration, line->reason);
		}

		if (!reading_db)
		{
			WriteDatabase();
		}
	}

	/** Called whenever an xline is deleted.
	 * This method is triggered after the line is deleted.
	 * @param source The user removing the line or NULL for local server
	 * @param line the line being deleted
	 */
	void OnDelLine(User* source, XLine* line)
	{
		for (std::vector<XLine *>::iterator i = xlines.begin(); i != xlines.end(); i++)
		{
			if ((*i) == line)
			{
				xlines.erase(i);
				break;
			}
		}

		WriteDatabase();
	}

	bool WriteDatabase()
	{
		FILE *f;

		/*
		 * We need to perform an atomic write so as not to fuck things up.
		 * So, let's write to a temporary file, flush and sync the FD, then rename the file..
		 * Technically, that means that this can block, but I have *never* seen that.
		 *		-- w00t
		 */
		ServerInstance->Log(DEBUG, "xlinedb: Opening temporary database");
		f = fopen("xline.db.new", "w");
		if (!f)
		{
			ServerInstance->Log(DEBUG, "xlinedb: Cannot create database! %s (%d)", strerror(errno), errno);
			ServerInstance->SNO->WriteToSnoMask('x', "database: cannot create new db: %s (%d)", strerror(errno), errno);
			return false;
		}

		ServerInstance->Log(DEBUG, "xlinedb: Opened. Writing..");

		/*
		 * Now, much as I hate writing semi-unportable formats, additional
		 * xline types may not have a conf tag, so let's just write them.
		 * In addition, let's use a file version, so we can maintain some
		 * semblance of backwards compatibility for reading on startup..
		 * 		-- w00t
		 */
		fprintf(f, "VERSION 1\n");

		// Now, let's write.
		XLine *line;
		for (std::vector<XLine *>::iterator i = xlines.begin(); i != xlines.end(); i++)
		{
			line = (*i);
			fprintf(f, "LINE %s %s %s %lu %lu :%s\n", line->type.c_str(), line->Displayable(),
				ServerInstance->Config->ServerName, line->set_time, line->duration, line->reason);
		}

		ServerInstance->Log(DEBUG, "xlinedb: Finished writing XLines. Checking for error..");

		int write_error = 0;
		write_error = ferror(f);
		write_error |= fclose(f);
		if (write_error)
		{
			ServerInstance->Log(DEBUG, "xlinedb: Cannot write to new database! %s (%d)", strerror(errno), errno);
			ServerInstance->SNO->WriteToSnoMask('x', "database: cannot write to new db: %s (%d)", strerror(errno), errno);
			return false;
		}

		// Use rename to move temporary to new db - this is guarenteed not to fuck up, even in case of a crash.
		if (rename("xline.db.new", "xline.db") < 0)
		{
			ServerInstance->Log(DEBUG, "xlinedb: Cannot move new to old database! %s (%d)", strerror(errno), errno);
			ServerInstance->SNO->WriteToSnoMask('x', "database: cannot replace old with new db: %s (%d)", strerror(errno), errno);
			return false;
		}

		return true;
	}

	bool ReadDatabase()
	{
		FILE *f;
		char linebuf[MAXBUF];
		unsigned int lineno = 0;

		f = fopen("xline.db", "r");
		if (!f)
		{
			if (errno == ENOENT)
			{
				/* xline.db doesn't exist, fake good return value (we don't care about this) */
				return true;
			}
			else
			{
				/* this might be slightly more problematic. */
				ServerInstance->Log(DEBUG, "xlinedb: Cannot read database! %s (%d)", strerror(errno), errno);
				ServerInstance->SNO->WriteToSnoMask('x', "database: cannot read db: %s (%d)", strerror(errno), errno);
				return false;
			}
		}

		while (fgets(linebuf, MAXBUF, f))
		{
			char *c = linebuf;

			while (c && *c)
			{
				if (*c == '\n')
				{
					*c = '\0';
				}

				c++;
			}
			// Smart man might think of initing to 1, and moving this to the bottom. Don't. We use continue in this loop.
			lineno++;

			// Inspired by the command parser. :)
			irc::tokenstream tokens(linebuf);
			int items = 0;
			std::string command_p[MAXPARAMETERS];
			std::string tmp;

			while (tokens.GetToken(tmp) && (items < MAXPARAMETERS))
			{
				command_p[items] = tmp.c_str();
				items++;
			}

			if (command_p[0] == "VERSION")
			{
				if (command_p[1] == "1")
				{
					ServerInstance->Log(DEBUG, "xlinedb: Reading db version %s", command_p[1].c_str());
				}
				else
				{
					fclose(f);
					ServerInstance->Log(DEBUG, "xlinedb: I got database version %s - I don't understand it", command_p[1].c_str());
					ServerInstance->SNO->WriteToSnoMask('x', "database: I got a database version (%s) I don't understand", command_p[1].c_str());
					return false;
				}
			}
			else if (command_p[0] == "LINE")
			{
				//mercilessly stolen from spanningtree
				XLineFactory* xlf = ServerInstance->XLines->GetFactory(command_p[0]);

				if (!xlf)
				{
					ServerInstance->SNO->WriteToSnoMask('x', "database: Unknown line type (%s).", command_p[1].c_str());
					continue;
				}

				XLine* xl = xlf->Generate(ServerInstance->Time(), atoi(command_p[5].c_str()), command_p[3].c_str(), command_p[6].c_str(), command_p[2].c_str());
				xl->SetCreateTime(atoi(command_p[4].c_str()));

				if (ServerInstance->XLines->AddLine(xl, NULL))
				{
					ServerInstance->SNO->WriteToSnoMask('x', "database: Added a line of type %s", command_p[1].c_str());
				}
			}
		}

		return true;
	}

	

	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleXLineDB)

