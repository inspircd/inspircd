/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Herman <GermanAizek@yandex.ru>
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2014 Justin Crawford <Justasic@Gmail.com>
 *   Copyright (C) 2013, 2015, 2018-2020, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012, 2014 Adam <Adam@anope.org>
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


#include "inspircd.h"
#include "xline.h"
#include <fstream>

class ModuleXLineDB
    : public Module
    , public Timer {
  private:
    bool dirty;
    std::string xlinedbpath;

  public:
    ModuleXLineDB()
        : Timer(0, true) {
    }

    void init() CXX11_OVERRIDE {
        /* Load the configuration
         * Note:
         *      This is on purpose not changed on a rehash. It would be non-trivial to change the database on-the-fly.
         *      Imagine a scenario where the new file already exists. Merging the current XLines with the existing database is likely a bad idea
         *      ...and so is discarding all current in-memory XLines for the ones in the database.
         */
        ConfigTag* Conf = ServerInstance->Config->ConfValue("xlinedb");
        xlinedbpath = ServerInstance->Config->Paths.PrependData(Conf->getString("filename", "xline.db", 1));
        SetInterval(Conf->getDuration("saveperiod", 5));

        // Read xlines before attaching to events
        ReadDatabase();

        dirty = false;
    }

    /** Called whenever an xline is added by a local user.
     * This method is triggered after the line is added.
     * @param source The sender of the line or NULL for local server
     * @param line The xline being added
     */
    void OnAddLine(User* source, XLine* line) CXX11_OVERRIDE {
        if (!line->from_config) {
            dirty = true;
        }
    }

    /** Called whenever an xline is deleted.
     * This method is triggered after the line is deleted.
     * @param source The user removing the line or NULL for local server
     * @param line the line being deleted
     */
    void OnDelLine(User* source, XLine* line) CXX11_OVERRIDE {
        OnAddLine(source, line);
    }

    bool Tick(time_t) CXX11_OVERRIDE {
        if (dirty) {
            if (WriteDatabase()) {
                dirty = false;
            }
        }
        return true;
    }

    bool WriteDatabase() {
        /*
         * We need to perform an atomic write so as not to fuck things up.
         * So, let's write to a temporary file, flush it, then rename the file..
         * Technically, that means that this can block, but I have *never* seen that.
         *     -- w00t
         */
        ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Opening temporary database");
        const std::string xlinenewdbpath = xlinedbpath + ".new." + ConvToStr(
                                               ServerInstance->Time());
        std::ofstream stream(xlinenewdbpath.c_str());
        if (!stream.is_open()) {
            ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                      "Cannot create database \"%s\"! %s (%d)", xlinenewdbpath.c_str(),
                                      strerror(errno), errno);
            ServerInstance->SNO->WriteToSnoMask('x',
                                                "database: cannot create new xline db \"%s\": %s (%d)", xlinenewdbpath.c_str(),
                                                strerror(errno), errno);
            return false;
        }

        ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Opened. Writing..");

        /*
         * Now, much as I hate writing semi-unportable formats, additional
         * xline types may not have a conf tag, so let's just write them.
         * In addition, let's use a file version, so we can maintain some
         * semblance of backwards compatibility for reading on startup..
         *      -- w00t
         */
        stream << "VERSION 1" << std::endl;

        // Now, let's write.
        std::vector<std::string> types = ServerInstance->XLines->GetAllTypes();
        for (std::vector<std::string>::const_iterator it = types.begin();
                it != types.end(); ++it) {
            XLineLookup* lookup = ServerInstance->XLines->GetAll(*it);
            if (!lookup) {
                continue;    // Not possible as we just obtained the list from XLineManager
            }

            for (LookupIter i = lookup->begin(); i != lookup->end(); ++i) {
                XLine* line = i->second;
                if (line->from_config) {
                    continue;
                }

                stream << "LINE " << line->type << " " << line->Displayable() << " "
                       << line->source << " " << line->set_time << " "
                       << line->duration << " :" << line->reason << std::endl;
            }
        }

        ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                  "Finished writing XLines. Checking for error..");

        if (stream.fail()) {
            ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                      "Cannot write to new database \"%s\"! %s (%d)", xlinenewdbpath.c_str(),
                                      strerror(errno), errno);
            ServerInstance->SNO->WriteToSnoMask('x',
                                                "database: cannot write to new xline db \"%s\": %s (%d)",
                                                xlinenewdbpath.c_str(), strerror(errno), errno);
            return false;
        }
        stream.close();

#ifdef _WIN32
        remove(xlinedbpath.c_str());
#endif
        // Use rename to move temporary to new db - this is guaranteed not to fuck up, even in case of a crash.
        if (rename(xlinenewdbpath.c_str(), xlinedbpath.c_str()) < 0) {
            ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                      "Cannot replace old database \"%s\" with new database \"%s\"! %s (%d)",
                                      xlinedbpath.c_str(), xlinenewdbpath.c_str(), strerror(errno), errno);
            ServerInstance->SNO->WriteToSnoMask('x',
                                                "database: cannot replace old xline db \"%s\" with new db \"%s\": %s (%d)",
                                                xlinedbpath.c_str(), xlinenewdbpath.c_str(), strerror(errno), errno);
            return false;
        }

        return true;
    }

    bool ReadDatabase() {
        // If the xline database doesn't exist then we don't need to load it.
        if (!FileSystem::FileExists(xlinedbpath)) {
            return true;
        }

        std::ifstream stream(xlinedbpath.c_str());
        if (!stream.is_open()) {
            ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                      "Cannot read database \"%s\"! %s (%d)", xlinedbpath.c_str(), strerror(errno),
                                      errno);
            ServerInstance->SNO->WriteToSnoMask('x',
                                                "database: cannot read xline db \"%s\": %s (%d)", xlinedbpath.c_str(),
                                                strerror(errno), errno);
            return false;
        }

        std::string line;
        while (std::getline(stream, line)) {
            // Inspired by the command parser. :)
            irc::tokenstream tokens(line);
            int items = 0;
            std::string command_p[7];
            std::string tmp;

            while (tokens.GetTrailing(tmp) && (items < 7)) {
                command_p[items] = tmp;
                items++;
            }

            ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Processing %s", line.c_str());

            if (command_p[0] == "VERSION") {
                if (command_p[1] != "1") {
                    stream.close();
                    ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                              "I got database version %s - I don't understand it", command_p[1].c_str());
                    ServerInstance->SNO->WriteToSnoMask('x',
                                                        "database: I got a database version (%s) I don't understand",
                                                        command_p[1].c_str());
                    return false;
                }
            } else if (command_p[0] == "LINE") {
                // Mercilessly stolen from spanningtree
                XLineFactory* xlf = ServerInstance->XLines->GetFactory(command_p[1]);

                if (!xlf) {
                    ServerInstance->SNO->WriteToSnoMask('x', "database: Unknown line type (%s).",
                                                        command_p[1].c_str());
                    continue;
                }

                XLine* xl = xlf->Generate(ServerInstance->Time(),
                                          ConvToNum<unsigned long>(command_p[5]), command_p[3], command_p[6],
                                          command_p[2]);
                xl->SetCreateTime(ConvToNum<time_t>(command_p[4]));

                if (ServerInstance->XLines->AddLine(xl, NULL)) {
                    ServerInstance->SNO->WriteToSnoMask('x', "database: Added a line of type %s",
                                                        command_p[1].c_str());
                } else {
                    delete xl;
                }
            }
        }
        stream.close();
        return true;
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows X-lines to be saved and reloaded on restart.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleXLineDB)
