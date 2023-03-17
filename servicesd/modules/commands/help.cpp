/* Core functions
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

#include "module.h"

class CommandHelp : public Command {
    static const unsigned help_wrap_len = 40;

    static CommandGroup *FindGroup(const Anope::string &name) {
        for (unsigned i = 0; i < Config->CommandGroups.size(); ++i) {
            CommandGroup &gr = Config->CommandGroups[i];
            if (gr.name == name) {
                return &gr;
            }
        }

        return NULL;
    }

  public:
    CommandHelp(Module *creator) : Command(creator, "generic/help", 0) {
        this->SetDesc(_("Displays this list and give information about commands"));
        this->AllowUnregistered(true);
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnPreHelp, MOD_RESULT, (source, params));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        Anope::string source_command = source.command;
        const BotInfo *bi = source.service;
        const CommandInfo::map &map = source.c ? Config->Fantasy : bi->commands;
        bool hide_privileged_commands = Config->GetBlock("options")->Get<bool>("hideprivilegedcommands"),
        hide_registered_commands = Config->GetBlock("options")->Get<bool>("hideregisteredcommands");

        if (params.empty() || params[0].equals_ci("ALL")) {
            bool all = !params.empty() && params[0].equals_ci("ALL");
            typedef std::map<CommandGroup *, std::list<Anope::string> > GroupInfo;
            GroupInfo groups;

            if (all) {
                source.Reply(_("All available commands for \002%s\002:"),
                             source.service->nick.c_str());
            }

            for (CommandInfo::map::const_iterator it = map.begin(), it_end = map.end();
                    it != it_end; ++it) {
                const Anope::string &c_name = it->first;
                const CommandInfo &info = it->second;

                if (info.hide) {
                    continue;
                }

                // Smaller command exists
                Anope::string cmd;
                spacesepstream(c_name).GetToken(cmd, 0);
                if (cmd != it->first && map.count(cmd)) {
                    continue;
                }

                ServiceReference<Command> c("Command", info.name);
                if (!c) {
                    continue;
                }

                if (hide_registered_commands && !c->AllowUnregistered()
                        && !source.GetAccount()) {
                    continue;
                }

                if (hide_privileged_commands && !info.permission.empty()
                        && !source.HasCommand(info.permission)) {
                    continue;
                }

                if (!info.group.empty() && !all) {
                    CommandGroup *gr = FindGroup(info.group);
                    if (gr != NULL) {
                        groups[gr].push_back(c_name);
                        continue;
                    }
                }

                source.command = c_name;
                c->OnServHelp(source);

            }

            for (GroupInfo::iterator it = groups.begin(), it_end = groups.end();
                    it != it_end; ++it) {
                CommandGroup *gr = it->first;

                source.Reply(" ");
                source.Reply("%s", gr->description.c_str());

                Anope::string buf;
                for (std::list<Anope::string>::iterator it2 = it->second.begin(),
                        it2_end = it->second.end(); it2 != it2_end; ++it2) {
                    const Anope::string &c_name = *it2;

                    buf += ", " + c_name;

                    if (buf.length() > help_wrap_len) {
                        source.Reply("  %s", buf.substr(2).c_str());
                        buf.clear();
                    }
                }
                if (buf.length() > 2) {
                    source.Reply("  %s", buf.substr(2).c_str());
                    buf.clear();
                }
            }
            if (!groups.empty()) {
                source.Reply(" ");
                source.Reply(
                    _("Use the \002%s ALL\002 command to list all commands and their descriptions."),
                    source_command.c_str());
            }
        } else {
            bool helped = false;
            for (unsigned max = params.size(); max > 0; --max) {
                Anope::string full_command;
                for (unsigned i = 0; i < max; ++i) {
                    full_command += " " + params[i];
                }
                full_command.erase(full_command.begin());

                CommandInfo::map::const_iterator it = map.find(full_command);
                if (it == map.end()) {
                    continue;
                }

                const CommandInfo &info = it->second;

                ServiceReference<Command> c("Command", info.name);
                if (!c) {
                    continue;
                }

                if (hide_privileged_commands && !info.permission.empty()
                        && !source.HasCommand(info.permission)) {
                    continue;
                }

                // Allow unregistered users to see help for commands that they explicitly request help for

                const Anope::string &subcommand = params.size() > max ? params[max] : "";
                source.command = it->first;
                if (!c->OnHelp(source, subcommand)) {
                    continue;
                }

                helped = true;

                /* Inform the user what permission is required to use the command */
                if (!info.permission.empty()) {
                    source.Reply(" ");
                    source.Reply(
                        _("Access to this command requires the permission \002%s\002 to be present in your opertype."),
                        info.permission.c_str());
                }
                if (!c->AllowUnregistered() && !source.nc) {
                    if (info.permission.empty()) {
                        source.Reply(" ");
                    }
                    source.Reply( _("You need to be identified to use this command."));
                }
                /* User doesn't have the proper permission to use this command */
                else if (!info.permission.empty() && !source.HasCommand(info.permission)) {
                    source.Reply(_("You cannot use this command."));
                }

                break;
            }

            if (helped == false) {
                source.Reply(_("No help available for \002%s\002."), params[0].c_str());
            }
        }

        FOREACH_MOD(OnPostHelp, (source, params));

        return;
    }
};

class Help : public Module {
    CommandHelp commandhelp;

  public:
    Help(const Anope::string &modname,
         const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandhelp(this) {

    }
};

MODULE_INIT(Help)
