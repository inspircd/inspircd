/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Sadie Powell <sadie@witchery.services>
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

/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModConfig: <eventexec event="startup|shutdown|rehash|link|unlink" command="command to execute goes here">
/// $ModDesc: Executes commands when a specified event occurs.
/// $ModDepends: core 3


#include "inspircd.h"
#include "modules/server.h"

enum EventType {
    ET_STARTUP,
    ET_SHUTDOWN,
    ET_REHASH,
    ET_SERVER_LINK,
    ET_SERVER_UNLINK
};

class CoreExport CommandThread : public Thread {
  private:
    std::vector<std::string> commands;

  public:
    CommandThread(const std::vector<std::string>& cmds)
        : commands(cmds) {
    }

    virtual ~CommandThread() {
        // Shuts the compiler up.
    }

    void Run() CXX11_OVERRIDE {
        for (std::vector<std::string>::const_iterator iter = commands.begin(); iter != commands.end(); ++iter) {
            system(iter->c_str());
        }

        ServerInstance->Threads.Stop(this);
        delete this;
    }
};


class ModuleEventExec
    : public Module
    , public ServerProtocol::LinkEventListener {
  private:
    // Maps event types to the associated commands.
    typedef insp::flat_multimap<EventType, std::string> EventMap;

    // Maps template vars to the associated values.
    typedef insp::flat_map<std::string, std::string, irc::insensitive_swo>
    TemplateMap;

    // The events which are currently registered.
    EventMap events;

    // Executes the events
    void ExecuteEvents(EventType type, const TemplateMap& map) {
        std::vector<std::string> commands;
        std::pair<EventMap::iterator, EventMap::iterator> iters = events.equal_range(
                    type);
        for (EventMap::iterator eiter = iters.first; eiter != iters.second; ++eiter) {
            std::string command = eiter->second;
            ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Formatting command: %s",
                                      command.c_str());

            size_t variable_start = std::string::npos;
            for (size_t cmdpos = 0; cmdpos < command.length(); ++cmdpos) {
                if (command[cmdpos] != '%') {
                    continue;
                }

                if (variable_start == std::string::npos) {
                    // We've found the start of a variable.
                    variable_start = cmdpos;
                    continue;
                }

                if ((cmdpos - variable_start) < 2) {
                    // Ignore empty variables (i.e. %%).
                    variable_start = std::string::npos;
                    continue;
                }

                // Extract the variable and replace it if it exists.
                const std::string variable = command.substr(variable_start + 1,
                                             cmdpos - variable_start - 1);
                TemplateMap::const_iterator titer = map.find(variable);
                if (titer == map.end()) {
                    ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                              "%s is not a valid variable for this event!", variable.c_str());
                    variable_start = std::string::npos;
                    continue;
                }

                ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                          "Replacing '%s' variable with '%s'", variable.c_str(), titer->second.c_str());
                command.replace(variable_start, cmdpos - variable_start + 1, titer->second);
                cmdpos = variable_start + titer->second.length();
                variable_start = std::string::npos;
            }

            ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                      "Scheduling command for execution: %s", command.c_str());
            commands.push_back(command);
        }

        // The thread will delete itself when done.
        ServerInstance->Threads.Start(new CommandThread(commands));
    }

  public:
    using ServerProtocol::LinkEventListener::OnServerSplit;

    ModuleEventExec()
        : ServerProtocol::LinkEventListener(this) {
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        EventMap newevents;
        ConfigTagList tags = ServerInstance->Config->ConfTags("eventexec");
        for (ConfigIter i = tags.first; i != tags.second; ++i) {
            ConfigTag* tag = i->second;

            // Ensure that we have the <eventexec:event> field.
            const std::string eventstr = tag->getString("event");
            if (eventstr.empty()) {
                throw ModuleException("<eventexec:event> is a required field, at " +
                                      tag->getTagLocation());
            }

            // Ensure that the <eventexec:event> value is well formed.
            EventType event;
            if (stdalgo::string::equalsci(eventstr, "startup")) {
                event = ET_STARTUP;
            } else if (stdalgo::string::equalsci(eventstr, "shutdown")) {
                event = ET_SHUTDOWN;
            } else if (stdalgo::string::equalsci(eventstr, "rehash")) {
                event = ET_REHASH;
            } else if (stdalgo::string::equalsci(eventstr, "link")) {
                event = ET_SERVER_LINK;
            } else if (stdalgo::string::equalsci(eventstr, "unlink")) {
                event = ET_SERVER_UNLINK;
            } else {
                throw ModuleException("<eventexec:event> contains an unrecognised event '" +
                                      eventstr + "', at " + tag->getTagLocation());
            }

            // Ensure that we have the <eventexec:command> parameter.
            const std::string command = tag->getString("command");
            if (command.empty()) {
                throw ModuleException("<eventexec:command> is a required field, at " +
                                      tag->getTagLocation());
            }

            newevents.insert(std::make_pair(event, command));
        }
        std::swap(newevents, events);

        if (status.initial) {
            ExecuteEvents(ET_STARTUP, TemplateMap());
        } else {
            TemplateMap map;
            map["user"] = status.srcuser ? status.srcuser->GetFullRealHost() :
                          ServerInstance->Config->ServerName;
            ExecuteEvents(ET_REHASH, map);
        }
    }

    void OnShutdown(const std::string& reason) CXX11_OVERRIDE {
        TemplateMap map;
        map["reason"] = reason;
        ExecuteEvents(ET_SHUTDOWN, map);
    }

    void OnServerLink(const Server* server) CXX11_OVERRIDE {
        TemplateMap map;
        map["id"] = server->GetId();
        map["name"] = server->GetName();
        ExecuteEvents(ET_SERVER_LINK, map);
    }

    void OnServerSplit(const Server* server, bool error) CXX11_OVERRIDE {
        TemplateMap map;
        map["error"] = error ? "yes" : "no";
        map["id"] = server->GetId();
        map["name"] = server->GetName();
        ExecuteEvents(ET_SERVER_UNLINK, map);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Executes commands when a specified event occurs");
    }
};

MODULE_INIT(ModuleEventExec)
