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

class CommandBSBot : public Command {
  private:
    void DoAdd(CommandSource &source, const std::vector<Anope::string> &params) {
        const Anope::string &nick = params[1];
        const Anope::string &user = params[2];
        const Anope::string &host = params[3];
        const Anope::string &real = params[4];

        if (BotInfo::Find(nick, true)) {
            source.Reply(_("Bot \002%s\002 already exists."), nick.c_str());
            return;
        }

        Configuration::Block *networkinfo = Config->GetBlock("networkinfo");

        if (nick.length() > networkinfo->Get<unsigned>("nicklen")) {
            source.Reply(_("Bot nicks may only be %d characters long."),
                         networkinfo->Get<unsigned>("nicklen"));
            return;
        }

        if (user.length() > networkinfo->Get<unsigned>("userlen")) {
            source.Reply(_("Bot idents may only be %d characters long."),
                         networkinfo->Get<unsigned>("userlen"));
            return;
        }

        if (host.length() > networkinfo->Get<unsigned>("hostlen")) {
            source.Reply(_("Bot hosts may only be %d characters long."),
                         networkinfo->Get<unsigned>("hostlen"));
            return;
        }

        if (!IRCD->IsNickValid(nick)) {
            source.Reply(_("Bot nicks may only contain valid nick characters."));
            return;
        }

        if (!IRCD->IsIdentValid(user)) {
            source.Reply(_("Bot idents may only contain valid ident characters."));
            return;
        }

        if (!IRCD->IsHostValid(host)) {
            source.Reply(_("Bot hosts may only contain valid host characters."));
            return;
        }

        /* We check whether the nick is registered, and inform the user
        * if so. You need to drop the nick manually before you can use
        * it as a bot nick from now on -GD
        */
        if (NickAlias::Find(nick)) {
            source.Reply(NICK_ALREADY_REGISTERED, nick.c_str());
            return;
        }

        User *u = User::Find(nick, true);
        if (u) {
            source.Reply(_("Nick \002%s\002 is currently in use."), u->nick.c_str());
            return;
        }

        BotInfo *bi = new BotInfo(nick, user, host, real);

        Log(LOG_ADMIN, source, this) << "ADD " << bi->GetMask() << " " << bi->realname;

        source.Reply(_("%s!%s@%s (%s) added to the bot list."), bi->nick.c_str(),
                     bi->GetIdent().c_str(), bi->host.c_str(), bi->realname.c_str());

        FOREACH_MOD(OnBotCreate, (bi));
        return;
    }

    void DoChange(CommandSource &source, const std::vector<Anope::string> &params) {
        const Anope::string &oldnick = params[1];
        const Anope::string &nick = params.size() > 2 ? params[2] : "";
        const Anope::string &user = params.size() > 3 ? params[3] : "";
        const Anope::string &host = params.size() > 4 ? params[4] : "";
        const Anope::string &real = params.size() > 5 ? params[5] : "";

        if (oldnick.empty() || nick.empty()) {
            this->OnSyntaxError(source, "CHANGE");
            return;
        }

        BotInfo *bi = BotInfo::Find(oldnick, true);
        if (!bi) {
            source.Reply(BOT_DOES_NOT_EXIST, oldnick.c_str());
            return;
        }

        if (bi->conf) {
            source.Reply(_("Bot %s is not changeable."), bi->nick.c_str());
            return;
        }

        Configuration::Block *networkinfo = Config->GetBlock("networkinfo");

        if (nick.length() > networkinfo->Get<unsigned>("nicklen")) {
            source.Reply(_("Bot nicks may only be %d characters long."),
                         networkinfo->Get<unsigned>("nicklen"));
            return;
        }

        if (user.length() > networkinfo->Get<unsigned>("userlen")) {
            source.Reply(_("Bot idents may only be %d characters long."),
                         networkinfo->Get<unsigned>("userlen"));
            return;
        }

        if (host.length() > networkinfo->Get<unsigned>("hostlen")) {
            source.Reply(_("Bot hosts may only be %d characters long."),
                         networkinfo->Get<unsigned>("hostlen"));
            return;
        }

        /* Checks whether there *are* changes.
        * Case sensitive because we may want to change just the case.
        * And we must finally check that the nick is not already
        * taken by another bot.
        */
        if (nick.equals_cs(bi->nick)
                && (!user.empty() ? user.equals_cs(bi->GetIdent()) : 1)
                && (!host.empty() ? host.equals_cs(bi->host) : 1)
                && (!real.empty() ? real.equals_cs(bi->realname) : 1)) {
            source.Reply(
                _("The old information is the same as the new information specified."));
            return;
        }

        if (!IRCD->IsNickValid(nick)) {
            source.Reply(_("Bot nicks may only contain valid nick characters."));
            return;
        }

        if (!user.empty() && !IRCD->IsIdentValid(user)) {
            source.Reply(_("Bot idents may only contain valid ident characters."));
            return;
        }

        if (!host.empty() && !IRCD->IsHostValid(host)) {
            source.Reply(_("Bot hosts may only contain valid host characters."));
            return;
        }

        if (!nick.equals_ci(bi->nick)) {
            if (BotInfo::Find(nick, true)) {
                source.Reply(_("Bot \002%s\002 already exists."), nick.c_str());
                return;
            }

            if (User::Find(nick, true)) {
                source.Reply(_("Nick \002%s\002 is currently in use."), nick.c_str());
                return;
            }
        }

        if (!nick.equals_ci(bi->nick)) {
            /* We check whether the nick is registered, and inform the user
            * if so. You need to drop the nick manually before you can use
            * it as a bot nick from now on -GD
            */
            if (NickAlias::Find(nick)) {
                source.Reply(NICK_ALREADY_REGISTERED, nick.c_str());
                return;
            }

            /* The new nick is really different, so we remove the Q line for the old nick. */
            XLine x_del(bi->nick);
            IRCD->SendSQLineDel(&x_del);

            /* Add a Q line for the new nick */
            XLine x(nick, "Reserved for services");
            IRCD->SendSQLine(NULL, &x);
        }

        if (!user.empty()) {
            IRCD->SendQuit(bi, "Quit: Be right back");
            bi->introduced = false;
        } else {
            IRCD->SendNickChange(bi, nick);
        }

        if (!nick.equals_cs(bi->nick)) {
            bi->SetNewNick(nick);
        }

        if (!user.empty() && !user.equals_cs(bi->GetIdent())) {
            bi->SetIdent(user);
        }
        if (!host.empty() && !host.equals_cs(bi->host)) {
            bi->host = host;
        }
        if (!real.empty() && !real.equals_cs(bi->realname)) {
            bi->realname = real;
        }

        if (!user.empty()) {
            bi->OnKill();
        }

        source.Reply(_("Bot \002%s\002 has been changed to %s!%s@%s (%s)."),
                     oldnick.c_str(), bi->nick.c_str(), bi->GetIdent().c_str(), bi->host.c_str(),
                     bi->realname.c_str());
        Log(LOG_ADMIN, source, this) << "CHANGE " << oldnick << " to " << bi->GetMask()
                                     << " " << bi->realname;

        FOREACH_MOD(OnBotChange, (bi));
        return;
    }

    void DoDel(CommandSource &source, const std::vector<Anope::string> &params) {
        const Anope::string &nick = params[1];

        if (nick.empty()) {
            this->OnSyntaxError(source, "DEL");
            return;
        }

        BotInfo *bi = BotInfo::Find(nick, true);
        if (!bi) {
            source.Reply(BOT_DOES_NOT_EXIST, nick.c_str());
            return;
        }

        if (bi->conf) {
            source.Reply(_("Bot %s is not deletable."), bi->nick.c_str());
            return;
        }

        FOREACH_MOD(OnBotDelete, (bi));

        Log(LOG_ADMIN, source, this) << "DEL " << bi->nick;

        source.Reply(_("Bot \002%s\002 has been deleted."), nick.c_str());
        delete bi;
        return;
    }
  public:
    CommandBSBot(Module *creator) : Command(creator, "botserv/bot", 1, 6) {
        this->SetDesc(_("Maintains network bot list"));
        this->SetSyntax(
            _("\002ADD \037nick\037 \037user\037 \037host\037 \037real\037\002"));
        this->SetSyntax(
            _("\002CHANGE \037oldnick\037 \037newnick\037 [\037user\037 [\037host\037 [\037real\037]]]\002"));
        this->SetSyntax(_("\002DEL \037nick\037\002"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &cmd = params[0];

        if (Anope::ReadOnly) {
            source.Reply(_("Sorry, bot modification is temporarily disabled."));
            return;
        }

        if (cmd.equals_ci("ADD")) {
            // ADD nick user host real - 5
            if (!source.HasCommand("botserv/bot/add")) {
                source.Reply(ACCESS_DENIED);
                return;
            }

            if (params.size() < 5) {
                this->OnSyntaxError(source, "ADD");
                return;
            }

            std::vector<Anope::string> tempparams = params;
            // ADD takes less params than CHANGE, so we need to take 6 if given and append it with a space to 5.
            if (tempparams.size() >= 6) {
                tempparams[4] = tempparams[4] + " " + tempparams[5];
            }

            return this->DoAdd(source, tempparams);
        } else if (cmd.equals_ci("CHANGE")) {
            // CHANGE oldn newn user host real - 6
            // but only oldn and newn are required
            if (!source.HasCommand("botserv/bot/change")) {
                source.Reply(ACCESS_DENIED);
                return;
            }

            if (params.size() < 3) {
                this->OnSyntaxError(source, "CHANGE");
                return;
            }

            return this->DoChange(source, params);
        } else if (cmd.equals_ci("DEL")) {
            // DEL nick
            if (!source.HasCommand("botserv/bot/del")) {
                source.Reply(ACCESS_DENIED);
                return;
            }

            if (params.size() < 1) {
                this->OnSyntaxError(source, "DEL");
                return;
            }

            return this->DoDel(source, params);
        } else {
            this->OnSyntaxError(source, "");
        }

        return;
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Allows Services Operators to create, modify, and delete\n"
                       "bots that users will be able to use on their own\n"
                       "channels.\n"
                       " \n"
                       "\002BOT ADD\002 adds a bot with the given nickname, username,\n"
                       "hostname and realname. Since no integrity checks are done\n"
                       "for these settings, be really careful.\n"
                       " \n"
                       "\002BOT CHANGE\002 allows you to change the nickname, username, hostname\n"
                       "or realname of a bot without deleting it (and\n"
                       "all the data associated with it).\n"
                       " \n"
                       "\002BOT DEL\002 removes the given bot from the bot list.\n"
                       " \n"
                       "\002Note\002: You cannot create a bot with a nick that is\n"
                       "currently registered. If an unregistered user is currently\n"
                       "using the nick, they will be killed."));
        return true;
    }
};

class BSBot : public Module {
    CommandBSBot commandbsbot;

  public:
    BSBot(const Anope::string &modname,
          const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandbsbot(this) {

    }
};

MODULE_INIT(BSBot)
