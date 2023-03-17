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

class CommandCSUnban : public Command {
  public:
    CommandCSUnban(Module *creator) : Command(creator, "chanserv/unban", 0, 2) {
        this->SetDesc(_("Remove all bans preventing a user from entering a channel"));
        this->SetSyntax(_("\037channel\037 [\037nick\037]"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        ChannelMode *cm = ModeManager::FindChannelModeByName("BAN");
        if (!cm) {
            return;
        }

        std::vector<ChannelMode *> modes = cm->listeners;
        modes.push_back(cm);

        if (params.empty()) {
            if (!source.GetUser()) {
                return;
            }

            std::deque<ChannelInfo *> queue;
            source.GetAccount()->GetChannelReferences(queue);

            unsigned count = 0;
            for (unsigned i = 0; i < queue.size(); ++i) {
                ChannelInfo *ci = queue[i];

                if (!ci->c || !source.AccessFor(ci).HasPriv("UNBAN")) {
                    continue;
                }

                FOREACH_MOD(OnChannelUnban, (source.GetUser(), ci));

                for (unsigned j = 0; j < modes.size(); ++j)
                    if (ci->c->Unban(source.GetUser(), modes[j]->name, true)) {
                        ++count;
                    }
            }

            Log(LOG_COMMAND, source, this, NULL) << "on all channels";
            source.Reply(_("You have been unbanned from %d channels."), count);

            return;
        }

        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        if (ci->c == NULL) {
            source.Reply(CHAN_X_NOT_IN_USE, ci->name.c_str());
            return;
        }

        if (!source.AccessFor(ci).HasPriv("UNBAN") && !source.HasPriv("chanserv/kick")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        User *u2 = source.GetUser();
        if (params.size() > 1) {
            u2 = User::Find(params[1], true);
        }

        if (!u2) {
            source.Reply(NICK_X_NOT_IN_USE, params[1].c_str());
            return;
        }

        bool override = !source.AccessFor(ci).HasPriv("UNBAN") && source.HasPriv("chanserv/kick");
        Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this, ci) << "to unban " << u2->nick;

        FOREACH_MOD(OnChannelUnban, (u2, ci));

        for (unsigned i = 0; i < modes.size(); ++i) {
            ci->c->Unban(u2, modes[i]->name, source.GetUser() == u2);
        }
        if (u2 == source.GetUser()) {
            source.Reply(_("You have been unbanned from \002%s\002."),
                         ci->c->name.c_str());
        } else {
            source.Reply(_("\002%s\002 has been unbanned from \002%s\002."),
                         u2->nick.c_str(), ci->c->name.c_str());
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Tells %s to remove all bans preventing you or the given\n"
                       "user from entering the given channel. If no channel is\n"
                       "given, all bans affecting you in channels you have access\n"
                       "in are removed.\n"
                       " \n"
                       "By default, limited to AOPs or those with level 5 access and above\n"
                       "on the channel."), source.service->nick.c_str());
        return true;
    }
};

class CSUnban : public Module {
    CommandCSUnban commandcsunban;

  public:
    CSUnban(const Anope::string &modname,
            const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandcsunban(this) {

    }
};

MODULE_INIT(CSUnban)
