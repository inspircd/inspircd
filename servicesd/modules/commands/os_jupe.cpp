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

class CommandOSJupe : public Command {
  public:
    CommandOSJupe(Module *creator) : Command(creator, "operserv/jupe", 1, 2) {
        this->SetDesc(_("\"Jupiter\" a server"));
        this->SetSyntax(_("\037server\037 [\037reason\037]"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &jserver = params[0];
        const Anope::string &reason = params.size() > 1 ? params[1] : "";
        Server *server = Server::Find(jserver, true);

        if (!IRCD->IsHostValid(jserver) || jserver.find('.') == Anope::string::npos) {
            source.Reply(_("Please use a valid server name when juping."));
        } else if (server == Me || server == Servers::GetUplink()) {
            source.Reply(
                _("You can not jupe your Services' pseudoserver or your uplink server."));
        } else if (server && server->IsJuped()) {
            source.Reply(_("You can not jupe an already juped server."));
        } else {
            Anope::string rbuf = "Juped by " + source.GetNick() + (!reason.empty() ? ": " +
                    reason : "");
            /* Generate the new sid before quitting the old server, so they can't collide */
            Anope::string sid = IRCD->SID_Retrieve();
            if (server) {
                IRCD->SendSquit(server, rbuf);
                server->Delete(rbuf);
            }
            Server *juped_server = new Server(Me, jserver, 1, rbuf, sid, true);
            IRCD->SendServer(juped_server);

            Log(LOG_ADMIN, source, this) << "on " << jserver << " (" << rbuf << ")";
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Tells Services to jupiter a server -- that is, to create\n"
                       "a fake \"server\" connected to Services which prevents\n"
                       "the real server of that name from connecting.  The jupe\n"
                       "may be removed using a standard \002SQUIT\002. If a reason is\n"
                       "given, it is placed in the server information field;\n"
                       "otherwise, the server information field will contain the\n"
                       "text \"Juped by <nick>\", showing the nickname of the\n"
                       "person who jupitered the server."));
        return true;
    }
};

class OSJupe : public Module {
    CommandOSJupe commandosjupe;

  public:
    OSJupe(const Anope::string &modname,
           const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandosjupe(this) {

    }
};

MODULE_INIT(OSJupe)
