/* BotServ core functions
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

class CommandBSSay : public Command {
  public:
    CommandBSSay(Module *creator) : Command(creator, "botserv/say", 2, 2) {
        this->SetDesc(
            _("Makes the bot say the specified text on the specified channel"));
        this->SetSyntax(_("\037channel\037 \037text\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &text = params[1];

        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        if (!source.AccessFor(ci).HasPriv("SAY") && !source.HasPriv("botserv/administration")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        if (!ci->bi) {
            source.Reply(BOT_NOT_ASSIGNED);
            return;
        }

        if (!ci->c || !ci->c->FindUser(ci->bi)) {
            source.Reply(BOT_NOT_ON_CHANNEL, ci->name.c_str());
            return;
        }

        if (text[0] == '\001') {
            this->OnSyntaxError(source, "");
            return;
        }

        IRCD->SendPrivmsg(*ci->bi, ci->name, "%s", text.c_str());
        ci->bi->lastmsg = Anope::CurTime;

        bool override = !source.AccessFor(ci).HasPriv("SAY");
        Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this, ci) << "to say: " << text;
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Makes the bot say the specified text on the specified channel."));
        return true;
    }
};

class CommandBSAct : public Command {
  public:
    CommandBSAct(Module *creator) : Command(creator, "botserv/act", 2, 2) {
        this->SetDesc(_("Makes the bot do the equivalent of a \"/me\" command"));
        this->SetSyntax(_("\037channel\037 \037text\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        Anope::string message = params[1];

        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        if (!source.AccessFor(ci).HasPriv("SAY") && !source.HasPriv("botserv/administration")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        if (!ci->bi) {
            source.Reply(BOT_NOT_ASSIGNED);
            return;
        }

        if (!ci->c || !ci->c->FindUser(ci->bi)) {
            source.Reply(BOT_NOT_ON_CHANNEL, ci->name.c_str());
            return;
        }

        message = message.replace_all_cs("\1", "");
        if (message.empty()) {
            return;
        }

        IRCD->SendAction(*ci->bi, ci->name, "%s", message.c_str());
        ci->bi->lastmsg = Anope::CurTime;

        bool override = !source.AccessFor(ci).HasPriv("SAY");
        Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this, ci) << "to say: " << message;
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Makes the bot do the equivalent of a \"/me\" command\n"
                       "on the specified channel using the specified text."));
        return true;
    }
};

class BSControl : public Module {
    CommandBSSay commandbssay;
    CommandBSAct commandbsact;

  public:
    BSControl(const Anope::string &modname,
              const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandbssay(this), commandbsact(this) {

    }
};

MODULE_INIT(BSControl)
