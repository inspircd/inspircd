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

class CommandOSKick : public Command {
  public:
    CommandOSKick(Module *creator) : Command(creator, "operserv/kick", 3, 3) {
        this->SetDesc(_("Kick a user from a channel"));
        this->SetSyntax(_("\037channel\037 \037user\037 \037reason\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &chan = params[0];
        const Anope::string &nick = params[1];
        const Anope::string &s = params[2];
        Channel *c;
        User *u2;

        if (!(c = Channel::Find(chan))) {
            source.Reply(CHAN_X_NOT_IN_USE, chan.c_str());
            return;
        }

        if (c->bouncy_modes) {
            source.Reply(
                _("Services is unable to change modes. Are your servers' U:lines configured correctly?"));
            return;
        }

        if (!(u2 = User::Find(nick, true))) {
            source.Reply(NICK_X_NOT_IN_USE, nick.c_str());
            return;
        }

        if (!c->Kick(source.service, u2, "%s (%s)", source.GetNick().c_str(), s.c_str())) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        Log(LOG_ADMIN, source, this) << "on " << u2->nick << " in " << c->name << " (" << s << ")";
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Allows staff to kick a user from any channel.\n"
                       "Parameters are the same as for the standard /KICK\n"
                       "command. The kick message will have the nickname of the\n"
                       "IRCop sending the KICK command prepended; for example:\n"
                       " \n"
                       "*** SpamMan has been kicked off channel #my_channel by %s (Alcan (Flood))"), source.service->nick.c_str());
        return true;
    }
};

class OSKick : public Module {
    CommandOSKick commandoskick;

  public:
    OSKick(const Anope::string &modname,
           const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandoskick(this) {

    }
};

MODULE_INIT(OSKick)
