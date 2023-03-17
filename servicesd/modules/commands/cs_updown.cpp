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

class CommandCSUp : public Command {
    void SetModes(User *u, Channel *c) {
        if (!c->ci) {
            return;
        }

        /* whether or not we are giving modes */
        bool giving = true;
        /* whether or not we have given a mode */
        bool given = false;
        AccessGroup u_access = c->ci->AccessFor(u);

        for (unsigned i = 0; i < ModeManager::GetStatusChannelModesByRank().size();
                ++i) {
            ChannelModeStatus *cm = ModeManager::GetStatusChannelModesByRank()[i];
            bool has_priv = u_access.HasPriv("AUTO" + cm->name)
                            || u_access.HasPriv(cm->name);

            if (has_priv) {
                /* Always give op. If we have already given one mode, don't give more until it has a symbol */
                if (cm->name == "OP" || !given || (giving && cm->symbol)) {
                    c->SetMode(NULL, cm, u->GetUID(), false);
                    /* Now if this contains a symbol don't give any more modes, to prevent setting +qaohv etc on users */
                    giving = !cm->symbol;
                    given = true;
                }
            }
        }
    }
  public:
    CommandCSUp(Module *creator) : Command(creator, "chanserv/up", 0, 2) {
        this->SetDesc(_("Updates a selected nicks status on a channel"));
        this->SetSyntax(_("[\037channel\037 [\037nick\037]]"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (params.empty()) {
            if (!source.GetUser()) {
                return;
            }
            for (User::ChanUserList::iterator it = source.GetUser()->chans.begin();
                    it != source.GetUser()->chans.end(); ++it) {
                Channel *c = it->second->chan;
                SetModes(source.GetUser(), c);
            }
            Log(LOG_COMMAND, source, this,
                NULL) << "on all channels to update their status modes";
        } else {
            const Anope::string &channel = params[0];
            const Anope::string &nick = params.size() > 1 ? params[1] : source.GetNick();

            Channel *c = Channel::Find(channel);

            if (c == NULL) {
                source.Reply(CHAN_X_NOT_IN_USE, channel.c_str());
                return;
            } else if (!c->ci) {
                source.Reply(CHAN_X_NOT_REGISTERED, channel.c_str());
                return;
            }

            User *u = User::Find(nick, true);
            User *srcu = source.GetUser();
            bool override = false;

            if (u == NULL) {
                source.Reply(NICK_X_NOT_IN_USE, nick.c_str());
                return;
            } else if (srcu && !srcu->FindChannel(c)) {
                source.Reply(_("You must be in \002%s\002 to use this command."),
                             c->name.c_str());
                return;
            } else if (!u->FindChannel(c)) {
                source.Reply(NICK_X_NOT_ON_CHAN, nick.c_str(), channel.c_str());
                return;
            } else if (source.GetUser() && u != source.GetUser()
                       && c->ci->HasExt("PEACE")) {
                if (c->ci->AccessFor(u) >= c->ci->AccessFor(source.GetUser())) {
                    if (source.HasPriv("chanserv/administration")) {
                        override = true;
                    } else {
                        source.Reply(ACCESS_DENIED);
                        return;
                    }
                }
            }

            Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this,
                c->ci) << "to update the status modes of " << u->nick;
            SetModes(u, c);
        }

    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Updates a selected nicks status modes on a channel. If \037nick\037 is\n"
                       "omitted then your status is updated. If \037channel\037 is omitted then\n"
                       "your channel status is updated on every channel you are in."));
        return true;
    }
};

class CommandCSDown : public Command {
    void RemoveAll(User *u, Channel *c) {
        ChanUserContainer *cu = c->FindUser(u);
        if (cu != NULL)
            for (size_t i = cu->status.Modes().length(); i > 0;) {
                c->RemoveMode(NULL, ModeManager::FindChannelModeByChar(cu->status.Modes()[--i]),
                              u->GetUID());
            }
    }

  public:
    CommandCSDown(Module *creator) : Command(creator, "chanserv/down", 0, 2) {
        this->SetDesc(_("Removes a selected nicks status from a channel"));
        this->SetSyntax(_("[\037channel\037 [\037nick\037]]"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (params.empty()) {
            if (!source.GetUser()) {
                return;
            }
            for (User::ChanUserList::iterator it = source.GetUser()->chans.begin();
                    it != source.GetUser()->chans.end(); ++it) {
                Channel *c = it->second->chan;
                RemoveAll(source.GetUser(), c);
            }
            Log(LOG_COMMAND, source, this,
                NULL) << "on all channels to remove their status modes";
        } else {
            const Anope::string &channel = params[0];
            const Anope::string &nick = params.size() > 1 ? params[1] : source.GetNick();

            Channel *c = Channel::Find(channel);

            if (c == NULL) {
                source.Reply(CHAN_X_NOT_IN_USE, channel.c_str());
                return;
            } else if (!c->ci) {
                source.Reply(CHAN_X_NOT_REGISTERED, channel.c_str());
                return;
            }

            User *u = User::Find(nick, true);
            User *srcu = source.GetUser();
            bool override = false;

            if (u == NULL) {
                source.Reply(NICK_X_NOT_IN_USE, nick.c_str());
                return;
            } else if (srcu && !srcu->FindChannel(c)) {
                source.Reply(_("You must be in \002%s\002 to use this command."),
                             c->name.c_str());
                return;
            } else if (!u->FindChannel(c)) {
                source.Reply(NICK_X_NOT_ON_CHAN, nick.c_str(), channel.c_str());
                return;
            } else if (source.GetUser() && u != source.GetUser()
                       && c->ci->HasExt("PEACE")) {
                if (c->ci->AccessFor(u) >= c->ci->AccessFor(source.GetUser())) {
                    if (source.HasPriv("chanserv/administration")) {
                        override = true;
                    } else {
                        source.Reply(ACCESS_DENIED);
                        return;
                    }
                }
            }

            Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this,
                c->ci) << "to remove the status modes from " << u->nick;
            RemoveAll(u, c);
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Removes a selected nicks status modes on a channel. If \037nick\037 is\n"
                       "omitted then your status is removed. If \037channel\037 is omitted then\n"
                       "your channel status is removed on every channel you are in."));
        return true;
    }
};

class CSUpDown : public Module {
    CommandCSUp commandcsup;
    CommandCSDown commandcsdown;

  public:
    CSUpDown(const Anope::string &modname,
             const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandcsup(this), commandcsdown(this) {

    }
};

MODULE_INIT(CSUpDown)
