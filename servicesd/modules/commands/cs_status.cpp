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

class CommandCSStatus : public Command {
  public:
    CommandCSStatus(Module *creator) : Command(creator, "chanserv/status", 1, 2) {
        this->SetDesc(_("Find a user's status on a channel"));
        this->SetSyntax(_("\037channel\037 [\037user\037]"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &channel = params[0];

        ChannelInfo *ci = ChannelInfo::Find(channel);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, channel.c_str());
        } else if (!source.AccessFor(ci).HasPriv("ACCESS_CHANGE") && !source.HasPriv("chanserv/auspex")) {
            source.Reply(ACCESS_DENIED);
        } else {
            Anope::string nick = source.GetNick();
            if (params.size() > 1) {
                nick = params[1];
            }

            AccessGroup ag;
            User *u = User::Find(nick, true);
            NickAlias *na = NULL;
            if (u != NULL) {
                ag = ci->AccessFor(u);
            } else {
                na = NickAlias::Find(nick);
                if (na != NULL) {
                    ag = ci->AccessFor(na->nc);
                }
            }

            if (ag.super_admin) {
                source.Reply(_("\002%s\002 is a super administrator."), nick.c_str());
            } else if (ag.founder) {
                source.Reply(_("\002%s\002 is the founder of \002%s\002."), nick.c_str(),
                             ci->name.c_str());
            } else  if (ag.empty()) {
                source.Reply(_("\002%s\002 has no access on \002%s\002."), nick.c_str(),
                             ci->name.c_str());
            } else {
                source.Reply(_("Access for \002%s\002 on \002%s\002:"), nick.c_str(),
                             ci->name.c_str());

                for (unsigned i = 0; i < ag.paths.size(); ++i) {
                    ChanAccess::Path &p = ag.paths[i];

                    if (p.empty()) {
                        continue;
                    }

                    if (p.size() == 1) {
                        ChanAccess *acc = p[0];

                        source.Reply(_("\002%s\002 matches access entry %s, which has privilege %s."),
                                     nick.c_str(), acc->Mask().c_str(), acc->AccessSerialize().c_str());
                    } else {
                        ChanAccess *first = p[0];
                        ChanAccess *acc = p[p.size() - 1];

                        source.Reply(
                            _("\002%s\002 matches access entry %s (from entry %s), which has privilege %s."),
                            nick.c_str(), acc->Mask().c_str(), first->Mask().c_str(),
                            acc->AccessSerialize().c_str());
                    }
                }
            }

            for (unsigned j = 0, end = ci->GetAkickCount(); j < end; ++j) {
                AutoKick *autokick = ci->GetAkick(j);

                if (autokick->nc) {
                    if (na && *autokick->nc == na->nc) {
                        source.Reply(_("\002%s\002 is on the auto kick list of \002%s\002 (%s)."),
                                     na->nc->display.c_str(), ci->name.c_str(), autokick->reason.c_str());
                    }
                } else if (u != NULL) {
                    Entry akick_mask("", autokick->mask);
                    if (akick_mask.Matches(u)) {
                        source.Reply(_("\002%s\002 matches auto kick entry %s on \002%s\002 (%s)."),
                                     u->nick.c_str(), autokick->mask.c_str(), ci->name.c_str(),
                                     autokick->reason.c_str());
                    }
                }
            }
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("This command tells you what a users access is on a channel\n"
                       "and what access entries, if any, they match. Additionally it\n"
                       "will tell you of any auto kick entries they match. Usage of\n"
                       "this command is limited to users who have the ability to modify\n"
                       "access entries on the channel."));
        return true;
    }
};

class CSStatus : public Module {
    CommandCSStatus commandcsstatus;

  public:
    CSStatus(const Anope::string &modname,
             const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandcsstatus(this) {
    }
};

MODULE_INIT(CSStatus)
