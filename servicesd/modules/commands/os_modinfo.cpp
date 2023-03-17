/* OperServ core functions
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

class CommandOSModInfo : public Command {
  public:
    CommandOSModInfo(Module *creator) : Command(creator, "operserv/modinfo", 1, 1) {
        this->SetDesc(_("Info about a loaded module"));
        this->SetSyntax(_("\037modname\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &file = params[0];

        Log(LOG_ADMIN, source, this) << "on " << file;

        Module *m = ModuleManager::FindModule(file);
        if (m) {
            source.Reply(
                _("Module: \002%s\002 Version: \002%s\002 Author: \002%s\002 Loaded: \002%s\002"),
                m->name.c_str(), !m->version.empty() ? m->version.c_str() : "?",
                !m->author.empty() ? m->author.c_str() : "Unknown", Anope::strftime(m->created,
                        source.GetAccount()).c_str());
            if (Anope::Debug) {
                source.Reply(_(" Loaded at: %p"), m->handle);
            }

            std::vector<Anope::string> servicekeys = Service::GetServiceKeys("Command");
            for (unsigned i = 0; i < servicekeys.size(); ++i) {
                ServiceReference<Command> c("Command", servicekeys[i]);
                if (!c || c->owner != m) {
                    continue;
                }

                source.Reply(_("   Providing service: \002%s\002"), c->name.c_str());

                for (botinfo_map::const_iterator it = BotListByNick->begin(),
                        it_end = BotListByNick->end(); it != it_end; ++it) {
                    const BotInfo *bi = it->second;

                    for (CommandInfo::map::const_iterator cit = bi->commands.begin(),
                            cit_end = bi->commands.end(); cit != cit_end; ++cit) {
                        const Anope::string &c_name = cit->first;
                        const CommandInfo &info = cit->second;
                        if (info.name != c->name) {
                            continue;
                        }
                        source.Reply(_("   Command \002%s\002 on \002%s\002 is linked to \002%s\002"),
                                     c_name.c_str(), bi->nick.c_str(), c->name.c_str());
                    }
                }
            }
        } else {
            source.Reply(_("No information about module \002%s\002 is available."),
                         file.c_str());
        }

        return;
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("This command lists information about the specified loaded module."));
        return true;
    }
};

class CommandOSModList : public Command {
  public:
    CommandOSModList(Module *creator) : Command(creator, "operserv/modlist", 0, 1) {
        this->SetDesc(_("List loaded modules"));
        this->SetSyntax("[all|third|vendor|extra|database|encryption|pseudoclient|protocol]");
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &param = !params.empty() ? params[0] : "";

        if (!param.empty()) {
            Log(LOG_ADMIN, source, this) << "for " << param;
        } else {
            Log(LOG_ADMIN, source, this);
        }

        bool third = false, vendor = false, extra = false, database = false, encryption = false, pseudoclient = false, protocol = false;

        if (param.equals_ci("all")) {
            third = vendor = extra = database = encryption = pseudoclient = protocol =
            true;
        } else if (param.equals_ci("third")) {
            third = true;
        } else if (param.equals_ci("vendor")) {
            vendor = true;
        } else if (param.equals_ci("extra")) {
            extra = true;
        } else if (param.equals_ci("database")) {
            database = true;
        } else if (param.equals_ci("encryption")) {
            encryption = true;
        } else if (param.equals_ci("pseudoclient")) {
            pseudoclient = true;
        } else if (param.equals_ci("protocol")) {
            protocol = true;
        } else {
            third = extra = database = encryption = protocol = true;
        }

        Module *protomod = ModuleManager::FindFirstOf(PROTOCOL);

        source.Reply(_("Current module list:"));

        int count = 0;
        for (std::list<Module *>::iterator it = ModuleManager::Modules.begin(), it_end = ModuleManager::Modules.end(); it != it_end; ++it) {
            Module *m = *it;

            bool show = false;
            Anope::string mtype;

            if (m->type & PROTOCOL) {
                show |= protocol;
                if (!mtype.empty()) {
                    mtype += ", ";
                }
                mtype += "Protocol";
            }
            if (m->type & PSEUDOCLIENT) {
                show |= pseudoclient;
                if (!mtype.empty()) {
                    mtype += ", ";
                }
                mtype += "Pseudoclient";
            }
            if (m->type & ENCRYPTION) {
                show |= encryption;
                if (!mtype.empty()) {
                    mtype += ", ";
                }
                mtype += "Encryption";
            }
            if (m->type & DATABASE) {
                show |= database;
                if (!mtype.empty()) {
                    mtype += ", ";
                }
                mtype += "Database";
            }
            if (m->type & EXTRA) {
                show |= extra;
                if (!mtype.empty()) {
                    mtype += ", ";
                }
                mtype += "Extra";
            }
            if (m->type & VENDOR) {
                show |= vendor;
                if (!mtype.empty()) {
                    mtype += ", ";
                }
                mtype += "Vendor";
            }
            if (m->type & THIRD) {
                show |= third;
                if (!mtype.empty()) {
                    mtype += ", ";
                }
                mtype += "Third";
            }

            if (!show) {
                continue;
            } else if (m->type & PROTOCOL && param.empty() && m != protomod) {
                continue;
            }

            ++count;

            source.Reply(_("Module: \002%s\002 [%s] [%s]"), m->name.c_str(),
                         m->version.c_str(), mtype.c_str());
        }

        if (!count) {
            source.Reply(_("No modules currently loaded matching that criteria."));
        } else {
            source.Reply(_("%d modules loaded."), count);
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Lists currently loaded modules."));
        return true;
    }
};

class OSModInfo : public Module {
    CommandOSModInfo commandosmodinfo;
    CommandOSModList commandosmodlist;

  public:
    OSModInfo(const Anope::string &modname,
              const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandosmodinfo(this), commandosmodlist(this) {

    }
};

MODULE_INIT(OSModInfo)
