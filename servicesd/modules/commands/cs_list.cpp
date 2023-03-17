/* ChanServ core functions
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
#include "modules/cs_mode.h"

class CommandCSList : public Command {
  public:
    CommandCSList(Module *creator) : Command(creator, "chanserv/list", 1, 2) {
        this->SetDesc(_("Lists all registered channels matching the given pattern"));
        this->SetSyntax(_("\037pattern\037 [SUSPENDED] [NOEXPIRE]"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        Anope::string pattern = params[0];
        unsigned nchans;
        bool is_servadmin = source.HasCommand("chanserv/list");
        int count = 0, from = 0, to = 0;
        bool suspended = false, channoexpire = false;

        if (pattern[0] == '#') {
            Anope::string n1, n2;
            sepstream(pattern.substr(1), '-').GetToken(n1, 0);
            sepstream(pattern, '-').GetToken(n2, 1);

            try {
                from = convertTo<int>(n1);
                to = convertTo<int>(n2);
            } catch (const ConvertException &) {
                source.Reply(LIST_INCORRECT_RANGE);
                source.Reply(
                    _("To search for channels starting with #, search for the channel\n"
                      "name without the #-sign prepended (\002anope\002 instead of \002#anope\002)."));
                return;
            }

            pattern = "*";
        }

        nchans = 0;

        if (is_servadmin && params.size() > 1) {
            Anope::string keyword;
            spacesepstream keywords(params[1]);
            while (keywords.GetToken(keyword)) {
                if (keyword.equals_ci("SUSPENDED")) {
                    suspended = true;
                }
                if (keyword.equals_ci("NOEXPIRE")) {
                    channoexpire = true;
                }
            }
        }

        Anope::string spattern = "#" + pattern;
        unsigned listmax = Config->GetModule(this->owner)->Get<unsigned>("listmax", "50");

        source.Reply(_("List of entries matching \002%s\002:"), pattern.c_str());

        ListFormatter list(source.GetAccount());
        list.AddColumn(_("Name")).AddColumn(_("Description"));

        Anope::map<ChannelInfo *> ordered_map;
        for (registered_channel_map::const_iterator it = RegisteredChannelList->begin(), it_end = RegisteredChannelList->end(); it != it_end; ++it) {
            ordered_map[it->first] = it->second;
        }

        for (Anope::map<ChannelInfo *>::const_iterator it = ordered_map.begin(), it_end = ordered_map.end(); it != it_end; ++it) {
            const ChannelInfo *ci = it->second;

            if (!is_servadmin) {
                if (ci->HasExt("CS_PRIVATE") || ci->HasExt("CS_SUSPENDED")) {
                    continue;
                }
                if (ci->c && ci->c->HasMode("SECRET")) {
                    continue;
                }

                ModeLocks *ml = ci->GetExt<ModeLocks>("modelocks");
                const ModeLock *secret = ml ? ml->GetMLock("SECRET") : NULL;
                if (secret && secret->set) {
                    continue;
                }
            }

            if (suspended && !ci->HasExt("CS_SUSPENDED")) {
                continue;
            }

            if (channoexpire && !ci->HasExt("CS_NO_EXPIRE")) {
                continue;
            }

            if (pattern.equals_ci(ci->name) || ci->name.equals_ci(spattern)
                    || Anope::Match(ci->name, pattern, false, true)
                    || Anope::Match(ci->name, spattern, false, true)
                    || Anope::Match(ci->desc, pattern, false, true)
                    || Anope::Match(ci->last_topic, pattern, false, true)) {
                if (((count + 1 >= from && count + 1 <= to) || (!from && !to))
                        && ++nchans <= listmax) {
                    bool isnoexpire = false;
                    if (is_servadmin && (ci->HasExt("CS_NO_EXPIRE"))) {
                        isnoexpire = true;
                    }

                    ListFormatter::ListEntry entry;
                    entry["Name"] = (isnoexpire ? "!" : "") + ci->name;
                    if (ci->HasExt("CS_SUSPENDED")) {
                        entry["Description"] = Language::Translate(source.GetAccount(),
                                               _("[Suspended]"));
                    } else {
                        entry["Description"] = ci->desc;
                    }
                    list.AddEntry(entry);
                }
                ++count;
            }
        }

        std::vector<Anope::string> replies;
        list.Process(replies);

        for (unsigned i = 0; i < replies.size(); ++i) {
            source.Reply(replies[i]);
        }

        source.Reply(_("End of list - %d/%d matches shown."), nchans > listmax ? listmax : nchans, nchans);
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Lists all registered channels matching the given pattern.\n"
                       "Channels with the \002PRIVATE\002 option set will only be\n"
                       "displayed to Services Operators with the proper access.\n"
                       "Channels with the \002NOEXPIRE\002 option set will have\n"
                       "a \002!\002 prefixed to the channel for Services Operators to see.\n"
                       " \n"
                       "Note that a preceding '#' specifies a range, channel names\n"
                       "are to be written without '#'.\n"
                       " \n"
                       "If the SUSPENDED or NOEXPIRE options are given, only channels\n"
                       "which, respectively, are SUSPENDED or have the NOEXPIRE\n"
                       "flag set will be displayed. If multiple options are given,\n"
                       "all channels matching at least one option will be displayed.\n"
                       "Note that these options are limited to \037Services Operators\037.\n"
                       " \n"
                       "Examples:\n"
                       " \n"
                       "    \002LIST *anope*\002\n"
                       "        Lists all registered channels with \002anope\002 in their\n"
                       "        names (case insensitive).\n"
                       " \n"
                       "    \002LIST * NOEXPIRE\002\n"
                       "        Lists all registered channels which have been set to not expire.\n"
                       " \n"
                       "    \002LIST #51-100\002\n"
                       "        Lists all registered channels within the given range (51-100)."));

        if (!Config->GetBlock("options")->Get<const Anope::string>("regexengine").empty()) {
            source.Reply(" ");
            source.Reply(_("Regex matches are also supported using the %s engine.\n"
                           "Enclose your pattern in // if this is desired."),
                         Config->GetBlock("options")->Get<const Anope::string>("regexengine").c_str());
        }

        return true;
    }
};

class CommandCSSetPrivate : public Command {
  public:
    CommandCSSetPrivate(Module *creator,
                        const Anope::string &cname = "chanserv/set/private") : Command(creator, cname,
                                    2, 2) {
        this->SetDesc(_("Hide channel from the LIST command"));
        this->SetSyntax(_("\037channel\037 {ON | OFF}"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetChannelOption, MOD_RESULT, (source, this, ci, params[1]));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        if (MOD_RESULT != EVENT_ALLOW && !source.AccessFor(ci).HasPriv("SET") && source.permission.empty() && !source.HasPriv("chanserv/administration")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        if (params[1].equals_ci("ON")) {
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to enable private";
            ci->Extend<bool>("CS_PRIVATE");
            source.Reply(_("Private option for %s is now \002on\002."), ci->name.c_str());
        } else if (params[1].equals_ci("OFF")) {
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to disable private";
            ci->Shrink<bool>("CS_PRIVATE");
            source.Reply(_("Private option for %s is now \002off\002."), ci->name.c_str());
        } else {
            this->OnSyntaxError(source, "PRIVATE");
        }

        return;
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Enables or disables the \002private\002 option for a channel."));

        BotInfo *bi;
        Anope::string cmd;
        if (Command::FindCommandFromService("chanserv/list", bi, cmd))
            source.Reply(_("When \002private\002 is set, the channel will not appear in\n"
                           "%s's %s command."), bi->nick.c_str(), cmd.c_str());
        return true;
    }
};

class CSList : public Module {
    CommandCSList commandcslist;
    CommandCSSetPrivate commandcssetprivate;

    SerializableExtensibleItem<bool> priv;

  public:
    CSList(const Anope::string &modname,
           const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandcslist(this), commandcssetprivate(this), priv(this, "CS_PRIVATE") {
    }

    void OnChanInfo(CommandSource &source, ChannelInfo *ci, InfoFormatter &info,
                    bool show_all) anope_override {
        if (!show_all) {
            return;
        }

        if (priv.HasExt(ci)) {
            info.AddOption(_("Private"));
        }
    }
};

MODULE_INIT(CSList)
