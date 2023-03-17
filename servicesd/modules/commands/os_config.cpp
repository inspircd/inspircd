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

class CommandOSConfig : public Command {
  public:
    CommandOSConfig(Module *creator) : Command(creator, "operserv/config", 1, 4) {
        this->SetDesc(_("View and change configuration file settings"));
        this->SetSyntax(
            _("{\037MODIFY\037|\037VIEW\037} [\037block name\037 \037item name\037 \037item value\037]"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &what = params[0];

        if (what.equals_ci("MODIFY") && params.size() > 3) {
            if (!source.HasPriv("operserv/config")) {
                source.Reply(ACCESS_DENIED);
                return;
            }

            Configuration::Block *block = Config->GetBlock(params[1]);
            if (!block) {
                block = Config->GetModule(params[1]);
            }

            if (!block) {
                source.Reply(_("There is no such configuration block %s."), params[1].c_str());
                return;
            }

            block->Set(params[2], params[3]);

            Log(LOG_ADMIN, source, this) << "to change the configuration value of " <<
                                         params[1] << ":" << params[2] << " to " << params[3];
            source.Reply(_("Value of %s:%s changed to %s"), params[1].c_str(),
                         params[2].c_str(), params[3].c_str());
        } else if (what.equals_ci("VIEW")) {
            /* Blocks we should show */
            const Anope::string show_blocks[] = { "serverinfo", "networkinfo", "options", "" };

            Log(LOG_ADMIN, source, this) << "VIEW";

            for (unsigned i = 0; !show_blocks[i].empty(); ++i) {
                Configuration::Block *block = Config->GetBlock(show_blocks[i]);
                const Configuration::Block::item_map *items = block->GetItems();

                if (!items) {
                    continue;
                }

                ListFormatter lflist(source.GetAccount());
                lflist.AddColumn(_("Name")).AddColumn(_("Value"));

                for (Configuration::Block::item_map::const_iterator it = items->begin(),
                        it_end = items->end(); it != it_end; ++it) {
                    ListFormatter::ListEntry entry;
                    entry["Name"] = it->first;
                    entry["Value"] = it->second;
                    lflist.AddEntry(entry);
                }

                std::vector<Anope::string> replies;
                lflist.Process(replies);

                source.Reply(_("%s settings:"), block->GetName().c_str());

                for (unsigned j = 0; j < replies.size(); ++j) {
                    source.Reply(replies[j]);
                }

                source.Reply(" ");
            }

            ListFormatter lflist(source.GetAccount());
            lflist.AddColumn(_("Module Name")).AddColumn(_("Name")).AddColumn(_("Value"));

            for (int i = 0; i < Config->CountBlock("module"); ++i) {
                Configuration::Block *block = Config->GetBlock("module", i);
                const Configuration::Block::item_map *items = block->GetItems();

                if (!items || items->size() <= 1) {
                    continue;
                }

                ListFormatter::ListEntry entry;
                entry["Module Name"] = block->Get<Anope::string>("name");

                for (Configuration::Block::item_map::const_iterator it = items->begin(),
                        it_end = items->end(); it != it_end; ++it) {
                    entry["Name"] = it->first;
                    entry["Value"] = it->second;
                    lflist.AddEntry(entry);
                }
            }

            std::vector<Anope::string> replies;
            lflist.Process(replies);

            source.Reply(_("Module settings:"));

            for (unsigned j = 0; j < replies.size(); ++j) {
                source.Reply(replies[j]);
            }

            source.Reply(_("End of configuration."));
        } else {
            this->OnSyntaxError(source, what);
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Allows you to change and view configuration settings.\n"
                       "Settings changed by this command are temporary and will not be reflected\n"
                       "back into the configuration file, and will be lost if Anope is shut down,\n"
                       "restarted, or the configuration is reloaded.\n"
                       " \n"
                       "Example:\n"
                       "     \002MODIFY nickserv forcemail no\002"));
        return true;
    }
};

class OSConfig : public Module {
    CommandOSConfig commandosconfig;

  public:
    OSConfig(const Anope::string &modname,
             const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandosconfig(this) {

    }
};

MODULE_INIT(OSConfig)
