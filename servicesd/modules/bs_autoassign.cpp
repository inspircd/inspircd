/*
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 */

#include "module.h"

class BSAutoAssign : public Module {
  public:
    BSAutoAssign(const Anope::string &modname,
                 const Anope::string &creator) : Module(modname, creator, VENDOR) {
    }

    void OnChanRegistered(ChannelInfo *ci) anope_override {
        const Anope::string &bot = Config->GetModule(this)->Get<const Anope::string>("bot");
        if (bot.empty()) {
            return;
        }

        BotInfo *bi = BotInfo::Find(bot, true);
        if (bi == NULL) {
            Log(this) << "bs_autoassign is configured to assign bot " << bot <<
                      ", but it does not exist?";
            return;
        }

        bi->Assign(NULL, ci);
    }
};

MODULE_INIT(BSAutoAssign)
