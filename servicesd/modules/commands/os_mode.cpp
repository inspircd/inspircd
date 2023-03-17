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

class CommandOSMode : public Command {
  public:
    CommandOSMode(Module *creator) : Command(creator, "operserv/mode", 2, 3) {
        this->SetDesc(_("Change channel modes"));
        this->SetSyntax(_("\037channel\037 \037modes\037"));
        this->SetSyntax(_("\037channel\037 CLEAR [ALL]"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &target = params[0];
        const Anope::string &modes = params[1];

        Reference<Channel> c = Channel::Find(target);
        if (!c) {
            source.Reply(CHAN_X_NOT_IN_USE, target.c_str());
        } else if (c->bouncy_modes) {
            source.Reply(
                _("Services is unable to change modes. Are your servers' U:lines configured correctly?"));
        } else if (modes.equals_ci("CLEAR")) {
            bool all = params.size() > 2 && params[2].equals_ci("ALL");

            const Channel::ModeList chmodes = c->GetModes();
            for (Channel::ModeList::const_iterator it = chmodes.begin(),
                    it_end = chmodes.end(); it != it_end && c; ++it) {
                c->RemoveMode(c->ci->WhoSends(), it->first, it->second, false);
            }

            if (!c) {
                source.Reply(_("Modes cleared on %s and the channel destroyed."),
                             target.c_str());
                return;
            }

            if (all) {
                for (Channel::ChanUserList::iterator it = c->users.begin(),
                        it_end = c->users.end(); it != it_end; ++it) {
                    ChanUserContainer *uc = it->second;

                    if (uc->user->HasMode("OPER")) {
                        continue;
                    }

                    for (size_t i = uc->status.Modes().length(); i > 0; --i) {
                        c->RemoveMode(c->ci->WhoSends(),
                                      ModeManager::FindChannelModeByChar(uc->status.Modes()[i - 1]),
                                      uc->user->GetUID(), false);
                    }
                }

                source.Reply(_("All modes cleared on %s."), c->name.c_str());
            } else {
                source.Reply(_("Non-status modes cleared on %s."), c->name.c_str());
            }
        } else {
            spacesepstream sep(modes + (params.size() > 2 ? " " + params[2] : ""));
            Anope::string mode;
            int add = 1;
            Anope::string log_modes, log_params;

            sep.GetToken(mode);
            for (unsigned i = 0; i < mode.length() && c; ++i) {
                char ch = mode[i];

                if (ch == '+') {
                    add = 1;
                    log_modes += "+";
                    continue;
                } else if (ch == '-') {
                    add = 0;
                    log_modes += "-";
                    continue;
                }

                ChannelMode *cm = ModeManager::FindChannelModeByChar(ch);
                if (!cm) {
                    continue;
                }

                Anope::string param, param_log;
                if (cm->type != MODE_REGULAR) {
                    if (cm->type == MODE_PARAM && !add
                            && anope_dynamic_static_cast<ChannelModeParam *>(cm)->minus_no_arg)
                        ;
                    else if (!sep.GetToken(param)) {
                        continue;
                    }

                    param_log = param;

                    if (cm->type == MODE_STATUS) {
                        User *targ = User::Find(param, true);
                        if (targ == NULL || c->FindUser(targ) == NULL) {
                            continue;
                        }
                        param = targ->GetUID();
                    }
                }

                log_modes += cm->mchar;
                if (!param.empty()) {
                    log_params += " " + param_log;
                }

                if (add) {
                    c->SetMode(source.service, cm, param, false);
                } else {
                    c->RemoveMode(source.service, cm, param, false);
                }
            }

            if (!log_modes.replace_all_cs("+", "").replace_all_cs("-", "").empty()) {
                Log(LOG_ADMIN, source, this) << log_modes << log_params << " on " <<
                                             (c ? c->name : target);
            }
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Allows Services Operators to change modes for any channel.\n"
                       "Parameters are the same as for the standard /MODE command.\n"
                       "Alternatively, CLEAR may be given to clear all modes on the channel.\n"
                       "If CLEAR ALL is given then all modes, including user status, is removed."));
        return true;
    }
};

class CommandOSUMode : public Command {
  public:
    CommandOSUMode(Module *creator) : Command(creator, "operserv/umode", 2, 2) {
        this->SetDesc(_("Change user modes"));
        this->SetSyntax(_("\037user\037 \037modes\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &target = params[0];
        const Anope::string &modes = params[1];

        User *u2 = User::Find(target, true);
        if (!u2) {
            source.Reply(NICK_X_NOT_IN_USE, target.c_str());
        } else {
            u2->SetModes(source.service, "%s", modes.c_str());
            source.Reply(_("Changed usermodes of \002%s\002 to %s."), u2->nick.c_str(),
                         modes.c_str());

            u2->SendMessage(source.service, _("\002%s\002 changed your usermodes to %s."),
                            source.GetNick().c_str(), modes.c_str());

            Log(LOG_ADMIN, source, this) << modes << " on " << target;
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Allows Services Operators to change modes for any user.\n"
                       "Parameters are the same as for the standard /MODE command."));
        return true;
    }
};

class OSMode : public Module {
    CommandOSMode commandosmode;
    CommandOSUMode commandosumode;

  public:
    OSMode(const Anope::string &modname,
           const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandosmode(this), commandosumode(this) {

    }
};

MODULE_INIT(OSMode)
