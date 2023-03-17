/* NickServ core functions
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

class CommandNSAList : public Command {
    static bool ChannelSort(ChannelInfo *ci1, ChannelInfo *ci2) {
        return ci::less()(ci1->name, ci2->name);
    }

  public:
    CommandNSAList(Module *creator) : Command(creator, "nickserv/alist", 0, 2) {
        this->SetDesc(_("List channels you have access on"));
        this->SetSyntax(_("[\037nickname\037]"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        Anope::string nick = source.GetNick();
        NickCore *nc = source.nc;

        if (params.size() && source.HasPriv("nickserv/alist")) {
            nick = params[0];
            const NickAlias *na = NickAlias::Find(nick);
            if (!na) {
                source.Reply(NICK_X_NOT_REGISTERED, nick.c_str());
                return;
            }
            nc = na->nc;
        }

        ListFormatter list(source.GetAccount());
        int chan_count = 0;

        list.AddColumn(_("Number")).AddColumn(_("Channel")).AddColumn(_("Access")).AddColumn(_("Description"));

        std::deque<ChannelInfo *> queue;
        nc->GetChannelReferences(queue);
        std::sort(queue.begin(), queue.end(), ChannelSort);

        for (unsigned i = 0; i < queue.size(); ++i) {
            ChannelInfo *ci = queue[i];
            ListFormatter::ListEntry entry;

            if (ci->GetFounder() == nc) {
                ++chan_count;
                entry["Number"] = stringify(chan_count);
                entry["Channel"] = (ci->HasExt("CS_NO_EXPIRE") ? "!" : "") + ci->name;
                entry["Access"] = Language::Translate(source.GetAccount(), _("Founder"));
                entry["Description"] = ci->desc;
                list.AddEntry(entry);
                continue;
            }

            if (ci->GetSuccessor() == nc) {
                ++chan_count;
                entry["Number"] = stringify(chan_count);
                entry["Channel"] = (ci->HasExt("CS_NO_EXPIRE") ? "!" : "") + ci->name;
                entry["Access"] = Language::Translate(source.GetAccount(), _("Successor"));
                entry["Description"] = ci->desc;
                list.AddEntry(entry);
                continue;
            }

            AccessGroup access = ci->AccessFor(nc, false);
            if (access.empty()) {
                continue;
            }

            ++chan_count;

            entry["Number"] = stringify(chan_count);
            entry["Channel"] = (ci->HasExt("CS_NO_EXPIRE") ? "!" : "") + ci->name;
            for (unsigned j = 0; j < access.paths.size(); ++j) {
                ChanAccess::Path &p = access.paths[j];

                // not interested in indirect access
                if (p.size() != 1) {
                    continue;
                }

                ChanAccess *a = p[0];
                entry["Access"] = entry["Access"] + ", " + a->AccessSerialize();
            }
            entry["Access"] = entry["Access"].substr(2);
            entry["Description"] = ci->desc;
            list.AddEntry(entry);
        }

        std::vector<Anope::string> replies;
        list.Process(replies);

        if (!chan_count) {
            source.Reply(_("\002%s\002 has no access in any channels."),
                         nc->display.c_str());
        } else {
            source.Reply(_("Channels that \002%s\002 has access on:"), nc->display.c_str());

            for (unsigned i = 0; i < replies.size(); ++i) {
                source.Reply(replies[i]);
            }

            source.Reply(_("End of list - %d channels shown."), chan_count);
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Lists all channels you have access on.\n"
                       " \n"
                       "Channels that have the \037NOEXPIRE\037 option set will be\n"
                       "prefixed by an exclamation mark. The nickname parameter is\n"
                       "limited to Services Operators"));

        return true;
    }
};

class NSAList : public Module {
    CommandNSAList commandnsalist;

  public:
    NSAList(const Anope::string &modname,
            const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandnsalist(this) {

    }
};

MODULE_INIT(NSAList)
