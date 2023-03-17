/*
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

class CommandBSSetGreet : public Command {
  public:
    CommandBSSetGreet(Module *creator,
                      const Anope::string &sname = "botserv/set/greet") : Command(creator, sname, 2,
                                  2) {
        this->SetDesc(_("Enable greet messages"));
        this->SetSyntax(_("\037channel\037 {\037ON|OFF\037}"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        const Anope::string &value = params[1];

        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        if (!source.HasPriv("botserv/administration") && !source.AccessFor(ci).HasPriv("SET")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        if (value.equals_ci("ON")) {
            bool override = !source.AccessFor(ci).HasPriv("SET");
            Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this,
                ci) << "to enable greets";

            ci->Extend<bool>("BS_GREET");
            source.Reply(_("Greet mode is now \002on\002 on channel %s."),
                         ci->name.c_str());
        } else if (value.equals_ci("OFF")) {
            bool override = !source.AccessFor(ci).HasPriv("SET");
            Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this,
                ci) << "to disable greets";

            ci->Shrink<bool>("BS_GREET");
            source.Reply(_("Greet mode is now \002off\002 on channel %s."),
                         ci->name.c_str());
        } else {
            this->OnSyntaxError(source, source.command);
        }
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(_(" \n"
                       "Enables or disables \002greet\002 mode on a channel.\n"
                       "When it is enabled, the bot will display greet\n"
                       "messages of users joining the channel, provided\n"
                       "they have enough access to the channel."));
        return true;
    }
};

class CommandNSSetGreet : public Command {
  public:
    CommandNSSetGreet(Module *creator,
                      const Anope::string &sname = "nickserv/set/greet",
                      size_t min = 0) : Command(creator, sname, min, min + 1) {
        this->SetDesc(_("Associate a greet message with your nickname"));
        this->SetSyntax(_("\037message\037"));
    }

    void Run(CommandSource &source, const Anope::string &user,
             const Anope::string &param) {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        const NickAlias *na = NickAlias::Find(user);
        if (!na) {
            source.Reply(NICK_X_NOT_REGISTERED, user.c_str());
            return;
        }
        NickCore *nc = na->nc;

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetNickOption, MOD_RESULT, (source, this, nc, param));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        if (!param.empty()) {
            Log(nc == source.GetAccount() ? LOG_COMMAND : LOG_ADMIN, source,
                this) << "to change the greet of " << nc->display;
            nc->Extend<Anope::string>("greet", param);
            source.Reply(_("Greet message for \002%s\002 changed to \002%s\002."),
                         nc->display.c_str(), param.c_str());
        } else {
            Log(nc == source.GetAccount() ? LOG_COMMAND : LOG_ADMIN, source,
                this) << "to unset the greet of " << nc->display;
            nc->Shrink<Anope::string>("greet");
            source.Reply(_("Greet message for \002%s\002 unset."), nc->display.c_str());
        }
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        this->Run(source, source.nc->display, params.size() > 0 ? params[0] : "");
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Makes the given message the greet of your nickname, that\n"
                       "will be displayed when joining a channel that has GREET\n"
                       "option enabled, provided that you have the necessary\n"
                       "access on it."));
        return true;
    }
};

class CommandNSSASetGreet : public CommandNSSetGreet {
  public:
    CommandNSSASetGreet(Module *creator) : CommandNSSetGreet(creator,
                "nickserv/saset/greet", 1) {
        this->ClearSyntax();
        this->SetSyntax(_("\037nickname\037 \037message\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        this->Run(source, params[0], params.size() > 1 ? params[1] : "");
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Makes the given message the greet of the nickname, that\n"
                       "will be displayed when joining a channel that has GREET\n"
                       "option enabled, provided that the user has the necessary\n"
                       "access on it."));
        return true;
    }
};

class Greet : public Module {
    /* channel setting for whether or not greet should be shown */
    SerializableExtensibleItem<bool> bs_greet;
    /* user greets */
    SerializableExtensibleItem<Anope::string> ns_greet;

    CommandBSSetGreet commandbssetgreet;
    CommandNSSetGreet commandnssetgreet;
    CommandNSSASetGreet commandnssasetgreet;

  public:
    Greet(const Anope::string &modname,
          const Anope::string &creator) : Module(modname, creator, VENDOR),
        bs_greet(this, "BS_GREET"),
        ns_greet(this, "greet"),
        commandbssetgreet(this),
        commandnssetgreet(this), commandnssasetgreet(this) {
    }

    void OnJoinChannel(User *user, Channel *c) anope_override {
        /* Only display the greet if the main uplink we're connected
         * to has synced, or we'll get greet-floods when the net
         * recovers from a netsplit. -GD
         */
        if (!c->ci || !c->ci->bi || !user->server->IsSynced() || !user->Account()) {
            return;
        }

        Anope::string *greet = ns_greet.Get(user->Account());
        if (bs_greet.HasExt(c->ci) && greet != NULL && !greet->empty() && c->FindUser(c->ci->bi) && c->ci->AccessFor(user).HasPriv("GREET")) {
            IRCD->SendPrivmsg(*c->ci->bi, c->name, "[%s] %s",
                              user->Account()->display.c_str(), greet->c_str());
            c->ci->bi->lastmsg = Anope::CurTime;
        }
    }

    void OnNickInfo(CommandSource &source, NickAlias *na, InfoFormatter &info,
                    bool show_hidden) anope_override {
        Anope::string *greet = ns_greet.Get(na->nc);
        if (greet != NULL) {
            info[_("Greet")] = *greet;
        }
    }

    void OnBotInfo(CommandSource &source, BotInfo *bi, ChannelInfo *ci,
                   InfoFormatter &info) anope_override {
        if (bs_greet.HasExt(ci)) {
            info.AddOption(_("Greet"));
        }
    }
};

MODULE_INIT(Greet)
