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

class CommandBSAssign : public Command {
  public:
    CommandBSAssign(Module *creator) : Command(creator, "botserv/assign", 2, 2) {
        this->SetDesc(_("Assigns a bot to a channel"));
        this->SetSyntax(_("\037channel\037 \037nick\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &chan = params[0];
        const Anope::string &nick = params[1];

        if (Anope::ReadOnly) {
            source.Reply(_("Sorry, bot assignment is temporarily disabled."));
            return;
        }

        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        BotInfo *bi = BotInfo::Find(nick, true);
        if (!bi) {
            source.Reply(BOT_DOES_NOT_EXIST, nick.c_str());
            return;
        }

        AccessGroup access = source.AccessFor(ci);
        if (ci->HasExt("BS_NOBOT") || (!access.HasPriv("ASSIGN") && !source.HasPriv("botserv/administration"))) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        if (bi->oper_only && !source.HasPriv("botserv/administration")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        if (ci->bi == bi) {
            source.Reply(_("Bot \002%s\002 is already assigned to channel \002%s\002."),
                         ci->bi->nick.c_str(), chan.c_str());
            return;
        }

        bool override = !access.HasPriv("ASSIGN");
        Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this, ci) << "for " << bi->nick;

        bi->Assign(source.GetUser(), ci);
        source.Reply(_("Bot \002%s\002 has been assigned to %s."), bi->nick.c_str(), ci->name.c_str());
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Assigns the specified bot to a channel. You\n"
                       "can then configure the bot for the channel so it fits\n"
                       "your needs."));
        return true;
    }
};

class CommandBSUnassign : public Command {
  public:
    CommandBSUnassign(Module *creator) : Command(creator, "botserv/unassign", 1,
                1) {
        this->SetDesc(_("Unassigns a bot from a channel"));
        this->SetSyntax(_("\037channel\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(_("Sorry, bot assignment is temporarily disabled."));
            return;
        }

        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        AccessGroup access = source.AccessFor(ci);
        if (!source.HasPriv("botserv/administration") && !access.HasPriv("ASSIGN")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        if (!ci->bi) {
            source.Reply(BOT_NOT_ASSIGNED);
            return;
        }

        if (ci->HasExt("PERSIST") && !ModeManager::FindChannelModeByName("PERM")) {
            source.Reply(
                _("You cannot unassign bots while persist is set on the channel."));
            return;
        }

        bool override = !access.HasPriv("ASSIGN");
        Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this, ci) << "for " << ci->bi->nick;

        ci->bi->UnAssign(source.GetUser(), ci);
        source.Reply(_("There is no bot assigned to %s anymore."), ci->name.c_str());
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Unassigns a bot from a channel. When you use this command,\n"
                       "the bot won't join the channel anymore. However, bot\n"
                       "configuration for the channel is kept, so you will always\n"
                       "be able to reassign a bot later without having to reconfigure\n"
                       "it entirely."));
        return true;
    }
};

class CommandBSSetNoBot : public Command {
  public:
    CommandBSSetNoBot(Module *creator,
                      const Anope::string &sname = "botserv/set/nobot") : Command(creator, sname, 2,
                                  2) {
        this->SetDesc(_("Prevent a bot from being assigned to a channel"));
        this->SetSyntax(_("\037channel\037 {\037ON|OFF\037}"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        const Anope::string &value = params[1];

        if (Anope::ReadOnly) {
            source.Reply(_("Sorry, bot modification is temporarily disabled."));
            return;
        }

        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        if (value.equals_ci("ON")) {
            Log(LOG_ADMIN, source, this, ci) << "to enable nobot";

            ci->Extend<bool>("BS_NOBOT");
            if (ci->bi) {
                ci->bi->UnAssign(source.GetUser(), ci);
            }
            source.Reply(_("No-bot mode is now \002on\002 on channel %s."),
                         ci->name.c_str());
        } else if (value.equals_ci("OFF")) {
            Log(LOG_ADMIN, source, this, ci) << "to disable nobot";

            ci->Shrink<bool>("BS_NOBOT");
            source.Reply(_("No-bot mode is now \002off\002 on channel %s."),
                         ci->name.c_str());
        } else {
            this->OnSyntaxError(source, source.command);
        }
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(_(" \n"
                       "This option makes a channel unassignable. If a bot\n"
                       "is already assigned to the channel, it is unassigned\n"
                       "automatically when you enable it."));
        return true;
    }
};

class BSAssign : public Module {
    ExtensibleItem<bool> nobot;

    CommandBSAssign commandbsassign;
    CommandBSUnassign commandbsunassign;
    CommandBSSetNoBot commandbssetnobot;

  public:
    BSAssign(const Anope::string &modname,
             const Anope::string &creator) : Module(modname, creator, VENDOR),
        nobot(this, "BS_NOBOT"),
        commandbsassign(this), commandbsunassign(this), commandbssetnobot(this) {
    }

    void OnInvite(User *source, Channel *c, User *targ) anope_override {
        BotInfo *bi;
        if (Anope::ReadOnly || !c->ci || targ->server != Me || !(bi = dynamic_cast<BotInfo *>(targ))) {
            return;
        }

        AccessGroup access = c->ci->AccessFor(source);
        if (nobot.HasExt(c->ci) || (!access.HasPriv("ASSIGN") && !source->HasPriv("botserv/administration"))) {
            targ->SendMessage(bi, ACCESS_DENIED);
            return;
        }

        if (bi->oper_only && !source->HasPriv("botserv/administration")) {
            targ->SendMessage(bi, ACCESS_DENIED);
            return;
        }

        if (c->ci->bi == bi) {
            targ->SendMessage(bi,
                              _("Bot \002%s\002 is already assigned to channel \002%s\002."),
                              c->ci->bi->nick.c_str(), c->name.c_str());
            return;
        }

        bi->Assign(source, c->ci);
        targ->SendMessage(bi, _("Bot \002%s\002 has been assigned to %s."), bi->nick.c_str(), c->name.c_str());
    }

    void OnBotInfo(CommandSource &source, BotInfo *bi, ChannelInfo *ci,
                   InfoFormatter &info) anope_override {
        if (nobot.HasExt(ci)) {
            info.AddOption(_("No bot"));
        }
    }
};

MODULE_INIT(BSAssign)
