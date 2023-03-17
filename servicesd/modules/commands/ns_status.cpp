/* NickServ core functions
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

class CommandNSStatus : public Command {
  public:
    CommandNSStatus(Module *creator) : Command(creator, "nickserv/status", 0, 16) {
        this->SetDesc(_("Returns the owner status of the given nickname"));
        this->SetSyntax(_("[\037nickname\037]"));
        this->AllowUnregistered(true);
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &nick = !params.empty() ? params[0] : source.GetNick();
        const NickAlias *na = NickAlias::Find(nick);
        spacesepstream sep(nick);
        Anope::string nickbuf;

        while (sep.GetToken(nickbuf)) {
            User *u2 = User::Find(nickbuf, true);
            if (!u2) { /* Nick is not online */
                source.Reply("STATUS %s %d %s", nickbuf.c_str(), 0, "");
            } else if (u2->IsIdentified() && na
                       && na->nc == u2->Account()) { /* Nick is identified */
                source.Reply("STATUS %s %d %s", nickbuf.c_str(), 3,
                             u2->Account()->display.c_str());
            } else if (u2->IsRecognized()) { /* Nick is recognised, but NOT identified */
                source.Reply("STATUS %s %d %s", nickbuf.c_str(), 2,
                             u2->Account() ? u2->Account()->display.c_str() : "");
            } else if (!na) { /* Nick is online, but NOT a registered */
                source.Reply("STATUS %s %d %s", nickbuf.c_str(), 0, "");
            } else
                /* Nick is not identified for the nick, but they could be logged into an account,
                 * so we tell the user about it
                 */
            {
                source.Reply("STATUS %s %d %s", nickbuf.c_str(), 1,
                             u2->Account() ? u2->Account()->display.c_str() : "");
            }
        }
        return;
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Returns whether the user using the given nickname is\n"
                       "recognized as the owner of the nickname. The response has\n"
                       "this format:\n"
                       " \n"
                       "    \037nickname\037 \037status-code\037 \037account\037\n"
                       " \n"
                       "where \037nickname\037 is the nickname sent with the command,\n"
                       "\037status-code\037 is one of the following, and \037account\037\n"
                       "is the account they are logged in as.\n"
                       " \n"
                       "    0 - no such user online \002or\002 nickname not registered\n"
                       "    1 - user not recognized as nickname's owner\n"
                       "    2 - user recognized as owner via access list only\n"
                       "    3 - user recognized as owner via password identification\n"
                       " \n"
                       "If no nickname is given, your status will be returned."));
        return true;
    }
};

class NSStatus : public Module {
    CommandNSStatus commandnsstatus;

  public:
    NSStatus(const Anope::string &modname,
             const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandnsstatus(this) {

    }
};

MODULE_INIT(NSStatus)
