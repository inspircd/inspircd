/* Global core functions
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

class CommandGLGlobal : public Command {
    ServiceReference<GlobalService> GService;

  public:
    CommandGLGlobal(Module *creator) : Command(creator, "global/global", 1, 1),
        GService("GlobalService", "Global") {
        this->SetDesc(_("Send a message to all users"));
        this->SetSyntax(_("\037message\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &msg = params[0];

        if (!GService) {
            source.Reply("No global reference, is global loaded?");
        } else {
            Log(LOG_ADMIN, source, this);
            GService->SendGlobal(NULL, source.GetNick(), msg);
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        Reference<BotInfo> sender;
        if (GService) {
            sender = GService->GetDefaultSender();
        }
        if (!sender) {
            sender = source.service;
        }

        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Allows Administrators to send messages to all users on the\n"
                       "network. The message will be sent from the nick \002%s\002."), sender->nick.c_str());
        return true;
    }
};

class GLGlobal : public Module {
    CommandGLGlobal commandglglobal;

  public:
    GLGlobal(const Anope::string &modname,
             const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandglglobal(this) {

    }
};

MODULE_INIT(GLGlobal)
