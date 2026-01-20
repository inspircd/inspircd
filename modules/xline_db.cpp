/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Herman <GermanAizek@yandex.ru>
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013, 2015, 2018-2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2014 Adam <Adam@anope.org>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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


#include <filesystem>
#include <fstream>

#include "inspircd.h"
#include "timeutils.h"
#include "xline.h"

class ModuleXLineDB final
	: public Module
	, public Timer
{
private:
	bool dirty;
	std::string xlinedbpath;
	unsigned long saveperiod;
	unsigned long maxbackoff;
	unsigned char backoff;

public:
	ModuleXLineDB()
		: Module(VF_VENDOR, "Allows X-lines to be saved and reloaded on restart.")
		, Timer(0, true)
	{
	}

	void init() override
	{
		/* Load the configuration
		 * Note:
		 *		This is on purpose not changed on a rehash. It would be non-trivial to change the database on-the-fly.
		 *		Imagine a scenario where the new file already exists. Merging the current XLines with the existing database is likely a bad idea
		 *		...and so is discarding all current in-memory XLines for the ones in the database.
		 */
		const auto& Conf = ServerInstance->Config->ConfValue("xlinedb");
		xlinedbpath = ServerInstance->Config->Paths.PrependData(Conf->getString("filename", "xline.db", 1));
		saveperiod = Conf->getDuration("saveperiod", 5);
		backoff = Conf->getNum<uint8_t>("backoff", 0);
		maxbackoff = Conf->getDuration("maxbackoff", saveperiod * 120, saveperiod);
		SetInterval(saveperiod);

		// Read xlines before attaching to events
		ReadDatabase();

		dirty = false;
	}

	/** Called whenever an xline is added by a local user.
	 * This method is triggered after the line is added.
	 * @param source The sender of the line or NULL for local server
	 * @param line The xline being added
	 */
	void OnAddLine(User* source, XLine* line) override
	{
		if (!line->from_config)
			dirty = true;
	}

	/** Called whenever an xline is deleted.
	 * This method is triggered after the line is deleted.
	 * @param source The user removing the line or NULL for local server
	 * @param line the line being deleted
	 */
	void OnDelLine(User* source, XLine* line) override
	{
		OnAddLine(source, line);
	}

	bool Tick() override
	{
		if (dirty)
		{
			if (WriteDatabase())
			{
				// If we were previously unable to write but now can then reset the time interval.
				if (GetInterval() != saveperiod)
					SetInterval(saveperiod, false);

				dirty = false;
			}
			else
			{
				// Back off a bit to avoid spamming opers.
				if (backoff > 1)
					SetInterval(std::min(GetInterval() * backoff, maxbackoff), false);
				ServerInstance->Logs.Debug(MODNAME, "Trying again in {}", Duration::ToLongString(GetInterval()));
			}
		}
		return true;
	}

	bool WriteDatabase()
	{
		/*
		 * We need to perform an atomic write so as not to fuck things up.
		 * So, let's write to a temporary file, flush it, then rename the file..
		 * Technically, that means that this can block, but I have *never* seen that.
		 *     -- w00t
		 */
		ServerInstance->Logs.Debug(MODNAME, "Opening temporary database");
		const auto xlinenewdbpath = FMT::format("{}.new.{}", xlinedbpath, ServerInstance->Time());
		std::ofstream stream(xlinenewdbpath);
		if (!stream.is_open())
		{
			ServerInstance->Logs.Critical(MODNAME, "Cannot create database \"{}\"! {} ({})", xlinenewdbpath, strerror(errno), errno);
			ServerInstance->SNO.WriteToSnoMask('x', "database: cannot create new xline db \"{}\": {} ({})", xlinenewdbpath, strerror(errno), errno);
			return false;
		}

		ServerInstance->Logs.Debug(MODNAME, "Opened. Writing..");

		/*
		 * Now, much as I hate writing semi-unportable formats, additional
		 * xline types may not have a conf tag, so let's just write them.
		 * In addition, let's use a file version, so we can maintain some
		 * semblance of backwards compatibility for reading on startup..
		 *		-- w00t
		 */
		stream << "VERSION 2" << std::endl;

		// Now, let's write.
		for (const auto& xltype : ServerInstance->XLines->GetAllTypes())
		{
			XLineLookup* lookup = ServerInstance->XLines->GetAll(xltype);
			if (!lookup)
				continue; // Not possible as we just obtained the list from XLineManager

			for (const auto& [_, line] : *lookup)
			{
				if (line->from_config)
					continue;

				std::string flags;
				if (line->local)
					flags.push_back('L');
				if (flags.empty())
					flags.push_back('*');

				stream << "LINE " << line->type << " " << line->Displayable() << " "
					<< line->source << " " << line->set_time << " "
					<< line->duration << " " << flags << " :" << line->reason << std::endl;
			}
		}

		ServerInstance->Logs.Debug(MODNAME, "Finished writing XLines. Checking for error..");

		if (stream.fail())
		{
			ServerInstance->Logs.Critical(MODNAME, "Cannot write to new database \"{}\"! {} ({})", xlinenewdbpath, strerror(errno), errno);
			ServerInstance->SNO.WriteToSnoMask('x', "database: cannot write to new xline db \"{}\": {} ({})", xlinenewdbpath, strerror(errno), errno);
			return false;
		}
		stream.close();

#ifdef _WIN32
		remove(xlinedbpath.c_str());
#endif
		// Use rename to move temporary to new db - this is guaranteed not to fuck up, even in case of a crash.
		if (rename(xlinenewdbpath.c_str(), xlinedbpath.c_str()) < 0)
		{
			ServerInstance->Logs.Critical(MODNAME, "Cannot replace old database \"{}\" with new database \"{}\"! {} ({})", xlinedbpath, xlinenewdbpath, strerror(errno), errno);
			ServerInstance->SNO.WriteToSnoMask('x', "database: cannot replace old xline db \"{}\" with new db \"{}\": {} ({})", xlinedbpath, xlinenewdbpath, strerror(errno), errno);
			return false;
		}

		return true;
	}

	bool ReadDatabase()
	{
		// If the xline database doesn't exist then we don't need to load it.
		std::error_code ec;
		if (!std::filesystem::is_regular_file(xlinedbpath, ec))
			return true;

		std::ifstream stream(xlinedbpath);
		if (!stream.is_open())
		{
			ServerInstance->Logs.Critical(MODNAME, "Cannot read database \"{}\"! {} ({})", xlinedbpath, strerror(errno), errno);
			ServerInstance->SNO.WriteToSnoMask('x', "database: cannot read xline db \"{}\": {} ({})", xlinedbpath, strerror(errno), errno);
			return false;
		}

		auto dbversion = 1;
		for (std::string line; std::getline(stream, line); )
		{
			ServerInstance->Logs.Debug(MODNAME, "Processing {}", line);

			// Inspired by the command parser. :)
			irc::tokenstream tokens(line);
			std::vector<std::string> command_p;
			for (std::string p; tokens.GetTrailing(p); )
				command_p.push_back(p);

			if (command_p[0] == "VERSION")
			{
				dbversion = ConvToNum<int>(command_p[1]);
				if (dbversion < 1 && dbversion > 2)
				{
					stream.close();
					ServerInstance->Logs.Critical(MODNAME, "I got database version {} - I don't understand it", version);
					ServerInstance->SNO.WriteToSnoMask('x', "database: I got a database version ({}) I don't understand", version);
					return false;
				}
			}
			else if (command_p[0] == "LINE")
			{
				// Mercilessly stolen from spanningtree
				XLineFactory* xlf = ServerInstance->XLines->GetFactory(command_p[1]);

				if (!xlf)
				{
					ServerInstance->SNO.WriteToSnoMask('x', "database: Unknown line type ({}).", command_p[1]);
					continue;
				}

				XLine* xl = xlf->Generate(ServerInstance->Time(), ConvToNum<unsigned long>(command_p[5]), command_p[3], command_p.back(), command_p[2]);
				xl->SetCreateTime(ConvToNum<time_t>(command_p[4]));

				if (dbversion >= 2)
				{
					// Flags are in command_p[6] in v2.
					for (const auto flag : command_p[6])
					{
						switch (flag)
						{
							case '*':
								break;
							case 'L':
								xl->local = true;
								break;
							default:
								ServerInstance->Logs.Debug(MODNAME, "Unknown xline flag: {}", flag);
								break;
						}
					}
				}

				if (!ServerInstance->XLines->AddLine(xl, nullptr))
				{
					continue;
					delete xl;
				}

				if (xl->duration)
				{
					ServerInstance->SNO.WriteToSnoMask('x', "database: added a timed {}{}{} on {}, expires in {} (on {}): {}",
						xl->local ? "local " : "", xl->type, xl->type.length() <= 2 ? "-line" : "", xl->Displayable(),
						Duration::ToLongString(xl->duration), Time::FromNow(xl->duration), xl->reason);
				}
				else
				{
					ServerInstance->SNO.WriteToSnoMask('x', "database: added a permanent {}{}{} on {}: {}", xl->type,
						xl->local ? "local " : "",  xl->type.length() <= 2 ? "-line" : "", xl->Displayable(), xl->reason);
				}
			}
		}
		stream.close();
		return true;
	}
};

MODULE_INIT(ModuleXLineDB)
