/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "xline.h"
#include <fstream>

class ModuleXLineDB : public Module
{
	bool dirty;
	std::string xlinedbpath;
 public:
	void init() CXX11_OVERRIDE
	{
		/* Load the configuration
		 * Note:
		 * 		this is on purpose not in the OnRehash() method. It would be non-trivial to change the database on-the-fly.
		 * 		Imagine a scenario where the new file already exists. Merging the current XLines with the existing database is likely a bad idea
		 * 		...and so is discarding all current in-memory XLines for the ones in the database.
		 */
		ConfigTag* Conf = ServerInstance->Config->ConfValue("xlinedb");
		xlinedbpath = ServerInstance->Config->Paths.PrependData(Conf->getString("filename", "xline.db"));

		// Read xlines before attaching to events
		ReadDatabase();

		dirty = false;
	}

	/** Called whenever an xline is added by a local user.
	 * This method is triggered after the line is added.
	 * @param source The sender of the line or NULL for local server
	 * @param line The xline being added
	 */
	void OnAddLine(User* source, XLine* line) CXX11_OVERRIDE
	{
		dirty = true;
	}

	/** Called whenever an xline is deleted.
	 * This method is triggered after the line is deleted.
	 * @param source The user removing the line or NULL for local server
	 * @param line the line being deleted
	 */
	void OnDelLine(User* source, XLine* line) CXX11_OVERRIDE
	{
		dirty = true;
	}

	void OnExpireLine(XLine *line) CXX11_OVERRIDE
	{
		dirty = true;
	}

	void OnBackgroundTimer(time_t now) CXX11_OVERRIDE
	{
		if (dirty)
		{
			if (WriteDatabase())
				dirty = false;
		}
	}

	bool WriteDatabase()
	{
		/*
		 * We need to perform an atomic write so as not to fuck things up.
		 * So, let's write to a temporary file, flush it, then rename the file..
		 * Technically, that means that this can block, but I have *never* seen that.
		 *     -- w00t
		 */
		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Opening temporary database");
		std::string xlinenewdbpath = xlinedbpath + ".new";
		std::ofstream stream(xlinenewdbpath.c_str());
		if (!stream.is_open())
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Cannot create database! %s (%d)", strerror(errno), errno);
			ServerInstance->SNO->WriteToSnoMask('a', "database: cannot create new db: %s (%d)", strerror(errno), errno);
			return false;
		}

		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Opened. Writing..");

		/*
		 * Now, much as I hate writing semi-unportable formats, additional
		 * xline types may not have a conf tag, so let's just write them.
		 * In addition, let's use a file version, so we can maintain some
		 * semblance of backwards compatibility for reading on startup..
		 * 		-- w00t
		 */
		stream << "VERSION 1" << std::endl;

		// Now, let's write.
		std::vector<std::string> types = ServerInstance->XLines->GetAllTypes();
		for (std::vector<std::string>::const_iterator it = types.begin(); it != types.end(); ++it)
		{
			XLineLookup* lookup = ServerInstance->XLines->GetAll(*it);
			if (!lookup)
				continue; // Not possible as we just obtained the list from XLineManager

			for (LookupIter i = lookup->begin(); i != lookup->end(); ++i)
			{
				XLine* line = i->second;
				stream << "LINE " << line->type << " " << line->Displayable() << " "
					<< ServerInstance->Config->ServerName << " " << line->set_time << " "
					<< line->duration << " " << line->reason << std::endl;
			}
		}

		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Finished writing XLines. Checking for error..");

		if (stream.fail())
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Cannot write to new database! %s (%d)", strerror(errno), errno);
			ServerInstance->SNO->WriteToSnoMask('a', "database: cannot write to new db: %s (%d)", strerror(errno), errno);
			return false;
		}
		stream.close();

#ifdef _WIN32
		if (remove(xlinedbpath.c_str()))
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Cannot remove old database! %s (%d)", strerror(errno), errno);
			ServerInstance->SNO->WriteToSnoMask('a', "database: cannot remove old database: %s (%d)", strerror(errno), errno);
			return false;
		}
#endif
		// Use rename to move temporary to new db - this is guarenteed not to fuck up, even in case of a crash.
		if (rename(xlinenewdbpath.c_str(), xlinedbpath.c_str()) < 0)
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Cannot move new to old database! %s (%d)", strerror(errno), errno);
			ServerInstance->SNO->WriteToSnoMask('a', "database: cannot replace old with new db: %s (%d)", strerror(errno), errno);
			return false;
		}

		return true;
	}

	bool ReadDatabase()
	{
		// If the xline database doesn't exist then we don't need to load it.
		if (!ServerConfig::FileExists(xlinedbpath.c_str()))
			return true;

		std::ifstream stream(xlinedbpath.c_str());
		if (!stream.is_open())
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Cannot read database! %s (%d)", strerror(errno), errno);
			ServerInstance->SNO->WriteToSnoMask('a', "database: cannot read db: %s (%d)", strerror(errno), errno);
			return false;
		}
		
		std::string line;
		while (std::getline(stream, line))
		{
			// Inspired by the command parser. :)
			irc::tokenstream tokens(line);
			int items = 0;
			std::string command_p[7];
			std::string tmp;

			while (tokens.GetToken(tmp) && (items < 7))
			{
				command_p[items] = tmp;
				items++;
			}

			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Processing %s", line.c_str());

			if (command_p[0] == "VERSION")
			{
				if (command_p[1] != "1")
				{
					stream.close();
					ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "I got database version %s - I don't understand it", command_p[1].c_str());
					ServerInstance->SNO->WriteToSnoMask('a', "database: I got a database version (%s) I don't understand", command_p[1].c_str());
					return false;
				}
			}
			else if (command_p[0] == "LINE")
			{
				// Mercilessly stolen from spanningtree
				XLineFactory* xlf = ServerInstance->XLines->GetFactory(command_p[1]);

				if (!xlf)
				{
					ServerInstance->SNO->WriteToSnoMask('a', "database: Unknown line type (%s).", command_p[1].c_str());
					continue;
				}

				XLine* xl = xlf->Generate(ServerInstance->Time(), atoi(command_p[5].c_str()), command_p[3], command_p[6], command_p[2]);
				xl->SetCreateTime(atoi(command_p[4].c_str()));

				if (ServerInstance->XLines->AddLine(xl, NULL))
				{
					ServerInstance->SNO->WriteToSnoMask('x', "database: Added a line of type %s", command_p[1].c_str());
				}
				else
					delete xl;
			}
		}
		stream.close();
		return true;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Keeps a dynamic log of all XLines created, and stores them in a separate conf file (xline.db).", VF_VENDOR);
	}
};

MODULE_INIT(ModuleXLineDB)
