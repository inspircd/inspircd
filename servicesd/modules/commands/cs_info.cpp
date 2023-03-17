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

class CommandCSInfo : public Command {
  public:
    CommandCSInfo(Module *creator) : Command(creator, "chanserv/info", 1, 2) {
        this->SetDesc(_("Lists information about the specified registered channel"));
        this->SetSyntax(_("\037channel\037"));
        this->AllowUnregistered(true);
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &chan = params[0];

        NickCore *nc = source.nc;
        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        bool has_auspex = source.HasPriv("chanserv/auspex");
        bool show_all = false;

        /* Should we show all fields? Only for sadmins and identified users */
        if (source.AccessFor(ci).HasPriv("INFO") || has_auspex) {
            show_all = true;
        }

        InfoFormatter info(nc);

        source.Reply(CHAN_INFO_HEADER, chan.c_str());
        if (ci->GetFounder()) {
            info[_("Founder")] = ci->GetFounder()->display;
        }

        if (show_all && ci->GetSuccessor()) {
            info[_("Successor")] = ci->GetSuccessor()->display;
        }

        if (!ci->desc.empty()) {
            info[_("Description")] = ci->desc;
        }

        info[_("Registered")] = Anope::strftime(ci->time_registered, source.GetAccount());
        info[_("Last used")] = Anope::strftime(ci->last_used, source.GetAccount());

        if (show_all) {
            info[_("Ban type")] = stringify(ci->bantype);
        }

        FOREACH_MOD(OnChanInfo, (source, ci, info, show_all));

        std::vector<Anope::string> replies;
        info.Process(replies);

        for (unsigned i = 0; i < replies.size(); ++i) {
            source.Reply(replies[i]);
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Lists information about the specified registered channel,\n"
                       "including its founder, time of registration, last\n"
                       "time used, and description. If the user issuing the\n"
                       "command has the appropriate access for it, then the\n"
                       "successor, last topic set, settings and expiration\n"
                       "time will also be displayed when applicable."));
        return true;
    }
};

class CSInfo : public Module {
    CommandCSInfo commandcsinfo;

  public:
    CSInfo(const Anope::string &modname,
           const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandcsinfo(this) {

    }
};

MODULE_INIT(CSInfo)
