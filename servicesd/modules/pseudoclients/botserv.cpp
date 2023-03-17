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

class BotServCore : public Module {
    Reference<BotInfo> BotServ;
    ExtensibleRef<bool> persist, inhabit;

  public:
    BotServCore(const Anope::string &modname,
                const Anope::string &creator) : Module(modname, creator, PSEUDOCLIENT | VENDOR),
        persist("PERSIST"), inhabit("inhabit") {
    }

    void OnReload(Configuration::Conf *conf) anope_override {
        const Anope::string &bsnick = conf->GetModule(this)->Get<const Anope::string>("client");
        BotServ = BotInfo::Find(bsnick, true);
    }

    void OnSetCorrectModes(User *user, Channel *chan, AccessGroup &access,
                           bool &give_modes, bool &take_modes) anope_override {
        /* Do not allow removing bot modes on our service bots */
        if (chan->ci && chan->ci->bi == user) {
            const Anope::string &botmodes = Config->GetModule(
                this)->Get<const Anope::string>("botmodes");
            for (unsigned i = 0; i < botmodes.length(); ++i) {
                chan->SetMode(chan->ci->bi, ModeManager::FindChannelModeByChar(botmodes[i]),
                              chan->ci->bi->GetUID());
            }
        }
    }

    void OnBotAssign(User *sender, ChannelInfo *ci, BotInfo *bi) anope_override {
        if (ci->c && ci->c->users.size() >= Config->GetModule(this)->Get<unsigned>("minusers")) {
            ChannelStatus status(Config->GetModule(
                                     this)->Get<const Anope::string>("botmodes"));
            bi->Join(ci->c, &status);
        }
    }

    void OnJoinChannel(User *user, Channel *c) anope_override {
        if (!Config || !IRCD) {
            return;
        }

        BotInfo *bi = user->server == Me ? dynamic_cast<BotInfo *>(user) : NULL;
        if (bi && Config->GetModule(this)->Get<bool>("smartjoin")) {
            std::vector<Anope::string> bans = c->GetModeList("BAN");

            /* We check for bans */
            for (unsigned int i = 0; i < bans.size(); ++i) {
                Entry ban("BAN", bans[i]);
                if (ban.Matches(user)) {
                    c->RemoveMode(NULL, "BAN", ban.GetMask());
                }
            }

            Anope::string Limit;
            unsigned limit = 0;
            try {
                if (c->GetParam("LIMIT", Limit)) {
                    limit = convertTo<unsigned>(Limit);
                }
            } catch (const ConvertException &) { }

            /* Should we be invited? */
            if (c->HasMode("INVITE") || (limit && c->users.size() >= limit)) {
                ChannelMode *cm = ModeManager::FindChannelModeByName("OP");
                char symbol = cm ? anope_dynamic_static_cast<ChannelModeStatus *>
                              (cm)->symbol : 0;
                IRCD->SendNotice(bi, (symbol ? Anope::string(symbol) : "") + c->name,
                                 "%s invited %s into the channel.", user->nick.c_str(), user->nick.c_str());
            }

            ModeManager::ProcessModes();
        }

        if (user->server != Me && c->ci && c->ci->bi) {
            /**
             * We let the bot join even if it was an ignored user, as if we don't,
             * and the ignored user doesn't just leave, the bot will never
             * make it into the channel, leaving the channel botless even for
             * legit users - Rob
             **/
            /* This is before the user has joined the channel, so check usercount + 1 */
            if (c->users.size() + 1 >= Config->GetModule(this)->Get<unsigned>("minusers")
                    && !c->FindUser(c->ci->bi)) {
                ChannelStatus status(Config->GetModule(
                                         this)->Get<const Anope::string>("botmodes"));
                c->ci->bi->Join(c, &status);
            }
        }
    }

    void OnLeaveChannel(User *u, Channel *c) anope_override {
        /* Channel is persistent, it shouldn't be deleted and the service bot should stay */
        if (c->ci && persist && persist->HasExt(c->ci)) {
            return;
        }

        /* Channel is syncing from a netburst, don't destroy it as more users are probably wanting to join immediately
         * We also don't part the bot here either, if necessary we will part it after the sync
         */
        if (c->syncing) {
            return;
        }

        /* Additionally, do not delete this channel if ChanServ/a BotServ bot is inhabiting it */
        if (inhabit && inhabit->HasExt(c)) {
            return;
        }

        /* This is called prior to removing the user from the channel, so c->users.size() - 1 should be safe */
        if (c->ci && c->ci->bi && u != *c->ci->bi && c->users.size() - 1 <= Config->GetModule(this)->Get<unsigned>("minusers") && c->FindUser(c->ci->bi)) {
            c->ci->bi->Part(c->ci->c);
        }
    }

    EventReturn OnPreHelp(CommandSource &source,
                          const std::vector<Anope::string> &params) anope_override {
        if (!params.empty()) {
            return EVENT_CONTINUE;
        }

        if (source.c) {
            source.Reply(
                _("\002%s\002 allows you to execute \"fantasy\" commands in the channel.\n"
                  "Fantasy commands are commands that can be executed from messaging a\n"
                  "channel, and provide a more convenient way to execute commands. Commands that\n"
                  "require a channel as a parameter will automatically have that parameter\n"
                  "given.\n"), source.service->nick.c_str());
            const Anope::string &fantasycharacters =
            Config->GetModule("fantasy")->Get<const Anope::string>("fantasycharacter", "!");
            if (!fantasycharacters.empty())
                source.Reply(_(" \n"
                               "Fantasy commands may be prefixed with one of the following characters: %s\n"),
                             fantasycharacters.c_str());
            source.Reply(_(" \n"
                           "Available commands are:"));
        } else if (*source.service == BotServ) {
            source.Reply(_("\002%s\002 allows you to have a bot on your own channel.\n"
                           "It has been created for users that can't host or\n"
                           "configure a bot, or for use on networks that don't\n"
                           "allow user bots. Available commands are listed\n"
                           "below; to use them, type \002%s%s \037command\037\002. For\n"
                           "more information on a specific command, type\n"
                           "\002%s%s %s \037command\037\002.\n"),
                         BotServ->nick.c_str(), Config->StrictPrivmsg.c_str(), BotServ->nick.c_str(),
                         Config->StrictPrivmsg.c_str(), BotServ->nick.c_str(), source.command.c_str());
        }

        return EVENT_CONTINUE;
    }

    void OnPostHelp(CommandSource &source,
                    const std::vector<Anope::string> &params) anope_override {
        if (!params.empty() || source.c || source.service != *BotServ) {
            return;
        }

        source.Reply(_(" \n"
                       "Bot will join a channel whenever there is at least\n"
                       "\002%d\002 user(s) on it."), Config->GetModule(this)->Get<unsigned>("minusers"));
        const Anope::string &fantasycharacters = Config->GetModule("fantasy")->Get<const Anope::string>("fantasycharacter", "!");
        if (!fantasycharacters.empty())
            source.Reply(_("Additionally, if fantasy is enabled fantasy commands\n"
                           "can be executed by prefixing the command name with\n"
                           "one of the following characters: %s"), fantasycharacters.c_str());
    }

    EventReturn OnChannelModeSet(Channel *c, MessageSource &source,
                                 ChannelMode *mode, const Anope::string &param) anope_override {
        if (source.GetUser() && !source.GetBot() && Config->GetModule(this)->Get<bool>("smartjoin") && mode->name == "BAN" && c->ci && c->ci->bi && c->FindUser(c->ci->bi)) {
            BotInfo *bi = c->ci->bi;

            Entry ban("BAN", param);
            if (ban.Matches(bi)) {
                c->RemoveMode(bi, "BAN", param);
            }
        }

        return EVENT_CONTINUE;
    }

    void OnCreateChan(ChannelInfo *ci) anope_override {
        /* Set default bot flags */
        spacesepstream sep(Config->GetModule(this)->Get<const Anope::string>("defaults", "greet fantasy"));
        for (Anope::string token; sep.GetToken(token);) {
            ci->Extend<bool>("BS_" + token.upper());
        }
    }

    void OnUserKicked(const MessageSource &source, User *target,
                      const Anope::string &channel, ChannelStatus &status,
                      const Anope::string &kickmsg) anope_override {
        BotInfo *bi = BotInfo::Find(target->GetUID());
        if (bi)
            /* Bots get rejoined */
        {
            bi->Join(channel, &status);
        }
    }

    void OnCreateBot(BotInfo *bi) anope_override {
        if (bi->botmodes.empty()) {
            bi->botmodes = Config->GetModule(this)->Get<const Anope::string>("botumodes");
        }
    }
};

MODULE_INIT(BotServCore)
