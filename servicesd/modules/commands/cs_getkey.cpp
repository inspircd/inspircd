/* ChanServ core functions
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

class CommandCSGetKey : public Command {
  public:
    CommandCSGetKey(Module *creator) : Command(creator, "chanserv/getkey", 1, 1) {
        this->SetDesc(_("Returns the key of the given channel"));
        this->SetSyntax(_("\037channel\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &chan = params[0];

        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        if (!source.AccessFor(ci).HasPriv("GETKEY") && !source.HasCommand("chanserv/getkey")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        Anope::string key;
        if (!ci->c || !ci->c->GetParam("KEY", key)) {
            source.Reply(_("Channel \002%s\002 has no key."), chan.c_str());
            return;
        }

        bool override = !source.AccessFor(ci).HasPriv("GETKEY");
        Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this, ci);

        source.Reply(_("Key for channel \002%s\002 is \002%s\002."), chan.c_str(), key.c_str());
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Returns the key of the given channel."));
        return true;
    }
};

class CSGetKey : public Module {
    CommandCSGetKey commandcsgetkey;

  public:
    CSGetKey(const Anope::string &modname,
             const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandcsgetkey(this) {

    }
};

MODULE_INIT(CSGetKey)
