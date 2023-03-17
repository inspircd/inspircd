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

class CommandCSInvite : public Command {
  public:
    CommandCSInvite(Module *creator) : Command(creator, "chanserv/invite", 1, 3) {
        this->SetDesc(_("Invites you or an optionally specified nick into a channel"));
        this->SetSyntax(_("\037channel\037 [\037nick\037]"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &chan = params[0];

        User *u = source.GetUser();
        Channel *c = Channel::Find(chan);

        if (!c) {
            source.Reply(CHAN_X_NOT_IN_USE, chan.c_str());
            return;
        }

        ChannelInfo *ci = c->ci;
        if (!ci) {
            source.Reply(CHAN_X_NOT_REGISTERED, chan.c_str());
            return;
        }

        if (!source.AccessFor(ci).HasPriv("INVITE") && !source.HasCommand("chanserv/invite")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        User *u2;
        if (params.size() == 1) {
            u2 = u;
        } else {
            u2 = User::Find(params[1], true);
        }

        if (!u2) {
            source.Reply(NICK_X_NOT_IN_USE,
                         params.size() > 1 ? params[1].c_str() : source.GetNick().c_str());
            return;
        }

        if (c->FindUser(u2)) {
            if (u2 == u) {
                source.Reply(_("You are already in \002%s\002!"), c->name.c_str());
            } else {
                source.Reply(_("\002%s\002 is already in \002%s\002!"), u2->nick.c_str(),
                             c->name.c_str());
            }
        } else {
            bool override = !source.AccessFor(ci).HasPriv("INVITE");

            IRCD->SendInvite(ci->WhoSends(), c, u2);
            if (u2 != u) {
                source.Reply(_("\002%s\002 has been invited to \002%s\002."), u2->nick.c_str(),
                             c->name.c_str());
                u2->SendMessage(ci->WhoSends(),
                                _("You have been invited to \002%s\002 by \002%s\002."), c->name.c_str(),
                                source.GetNick().c_str());
                Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this,
                    ci) << "for " << u2->nick;
            } else {
                u2->SendMessage(ci->WhoSends(), _("You have been invited to \002%s\002."),
                                c->name.c_str());
                Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this, ci);
            }
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Tells %s to invite you or an optionally specified\n"
                       "nick into the given channel.\n"
                       " \n"
                       "By default, limited to AOPs or those with level 5 access and above\n"
                       "on the channel."), source.service->nick.c_str());
        return true;
    }
};

class CSInvite : public Module {
    CommandCSInvite commandcsinvite;

  public:
    CSInvite(const Anope::string &modname,
             const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandcsinvite(this) {

    }
};

MODULE_INIT(CSInvite)
