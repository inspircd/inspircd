/*
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

#include "module.h"

class HelpChannel : public Module {
  public:
    HelpChannel(const Anope::string &modname,
                const Anope::string &creator) : Module(modname, creator, VENDOR) {
    }

    EventReturn OnChannelModeSet(Channel *c, MessageSource &, ChannelMode *mode,
                                 const Anope::string &param) anope_override {
        if (mode->name == "OP" && c && c->ci && c->name.equals_ci(Config->GetModule(this)->Get<const Anope::string>("helpchannel"))) {
            User *u = User::Find(param);

            if (u && c->ci->AccessFor(u).HasPriv("OPME")) {
                u->SetMode(Config->GetClient("OperServ"), "HELPOP");
            }
        }

        return EVENT_CONTINUE;
    }
};

MODULE_INIT(HelpChannel)
