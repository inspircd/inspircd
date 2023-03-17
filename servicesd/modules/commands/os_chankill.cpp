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

static ServiceReference<XLineManager> akills("XLineManager",
        "xlinemanager/sgline");

class CommandOSChanKill : public Command {
  public:
    CommandOSChanKill(Module *creator) : Command(creator, "operserv/chankill", 2,
                3) {
        this->SetDesc(_("AKILL all users on a specific channel"));
        this->SetSyntax(_("[+\037expiry\037] \037channel\037 \037reason\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (!akills) {
            return;
        }

        Anope::string expiry, channel;
        unsigned last_param = 1;
        Channel *c;

        channel = params[0];
        if (!channel.empty() && channel[0] == '+') {
            expiry = channel;
            channel = params[1];
            last_param = 2;
        }

        time_t expires = !expiry.empty() ? Anope::DoTime(expiry) : Config->GetModule("operserv")->Get<time_t>("autokillexpiry", "30d");
        if (!expiry.empty() && isdigit(expiry[expiry.length() - 1])) {
            expires *= 86400;
        }
        if (expires && expires < 60) {
            source.Reply(BAD_EXPIRY_TIME);
            return;
        } else if (expires > 0) {
            expires += Anope::CurTime;
        }

        if (params.size() <= last_param) {
            this->OnSyntaxError(source, "");
            return;
        }

        Anope::string reason = params[last_param];
        if (params.size() > last_param + 1) {
            reason += params[last_param + 1];
        }
        if (!reason.empty()) {
            Anope::string realreason;
            if (Config->GetModule("operserv")->Get<bool>("addakiller")
                    && !source.GetNick().empty()) {
                realreason = "[" + source.GetNick() + "] " + reason;
            } else {
                realreason = reason;
            }

            if ((c = Channel::Find(channel))) {
                for (Channel::ChanUserList::iterator it = c->users.begin(),
                        it_end = c->users.end(); it != it_end; ++it) {
                    ChanUserContainer *uc = it->second;

                    if (uc->user->server == Me || uc->user->HasMode("OPER")) {
                        continue;
                    }

                    Anope::string akillmask = "*@" + uc->user->host;
                    if (akills->HasEntry(akillmask)) {
                        continue;
                    }

                    XLine *x = new XLine(akillmask, source.GetNick(), expires, realreason,
                                         XLineManager::GenerateUID());
                    akills->AddXLine(x);
                    akills->OnMatch(uc->user, x);
                }

                Log(LOG_ADMIN, source, this) << "on " << c->name << " (" << realreason << ")";
            } else {
                source.Reply(CHAN_X_NOT_IN_USE, channel.c_str());
            }
        }
        return;
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Puts an AKILL for every nick on the specified channel. It\n"
                       "uses the entire real ident@host for every nick, and\n"
                       "then enforces the AKILL."));
        return true;
    }
};

class OSChanKill : public Module {
    CommandOSChanKill commandoschankill;

  public:
    OSChanKill(const Anope::string &modname,
               const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandoschankill(this) {

    }
};

MODULE_INIT(OSChanKill)
