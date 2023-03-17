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
#include "modules/cs_mode.h"

inline static Anope::string BotModes() {
    return Config->GetModule("botserv")->Get<Anope::string>("botmodes",
            Config->GetModule("chanserv")->Get<Anope::string>("botmodes", "o")
                                                           );
}

class ChanServCore : public Module, public ChanServService {
    Reference<BotInfo> ChanServ;
    std::vector<Anope::string> defaults;
    ExtensibleItem<bool> inhabit;
    ExtensibleRef<bool> persist;
    bool always_lower;

  public:
    ChanServCore(const Anope::string &modname,
                 const Anope::string &creator) : Module(modname, creator, PSEUDOCLIENT | VENDOR),
        ChanServService(this), inhabit(this, "inhabit"), persist("PERSIST"),
        always_lower(false) {
    }

    void Hold(Channel *c) anope_override {
        /** A timer used to keep the BotServ bot/ChanServ in the channel
         * after kicking the last user in a channel
         */
        class ChanServTimer : public Timer
        {
            Reference<BotInfo> &ChanServ;
            ExtensibleItem<bool> &inhabit;
            Reference<Channel> c;

          public:
            /** Constructor
             * @param chan The channel
             */
            ChanServTimer(Reference<BotInfo> &cs, ExtensibleItem<bool> &i, Module *m, Channel *chan) : Timer(m, Config->GetModule(m)->Get<time_t>("inhabit", "15s")), ChanServ(cs), inhabit(i), c(chan) {
                if (!ChanServ || !c) {
                    return;
                }
                inhabit.Set(c, true);
                if (!c->ci || !c->ci->bi) {
                    ChanServ->Join(c);
                } else if (!c->FindUser(c->ci->bi)) {
                    c->ci->bi->Join(c);
                }

                /* Set +ntsi to prevent rejoin */
                c->SetMode(NULL, "NOEXTERNAL");
                c->SetMode(NULL, "TOPIC");
                c->SetMode(NULL, "SECRET");
                c->SetMode(NULL, "INVITE");
            }

            /** Called when the delay is up
             * @param The current time
             */
            void Tick(time_t) anope_override
            {
                if (!c) {
                    return;
                }

                /* In the event we don't part */
                c->RemoveMode(NULL, "SECRET");
                c->RemoveMode(NULL, "INVITE");

                inhabit.Unset(c); /* now we're done changing modes, unset inhabit */

                if (!c->ci || !c->ci->bi) {
                    if (ChanServ) {
                        ChanServ->Part(c);
                    }
                }
                /* If someone has rejoined this channel in the meantime, don't part the bot */
                else if (c->users.size() <= 1) {
                    c->ci->bi->Part(c);
                }
            }
        };

        if (inhabit.HasExt(c)) {
            return;
        }

        new ChanServTimer(ChanServ, inhabit, this->owner, c);
    }

    void OnReload(Configuration::Conf *conf) anope_override {
        const Anope::string &channick = conf->GetModule(this)->Get<const Anope::string>("client");

        if (channick.empty()) {
            throw ConfigException(Module::name + ": <client> must be defined");
        }

        BotInfo *bi = BotInfo::Find(channick, true);
        if (!bi) {
            throw ConfigException(Module::name + ": no bot named " + channick);
        }

        ChanServ = bi;

        spacesepstream(conf->GetModule(this)->Get<const Anope::string>("defaults", "greet fantasy")).GetTokens(defaults);
        if (defaults.empty()) {
            defaults.push_back("KEEPTOPIC");
            defaults.push_back("CS_SECURE");
            defaults.push_back("SECUREFOUNDER");
            defaults.push_back("SIGNKICK");
        } else if (defaults[0].equals_ci("none")) {
            defaults.clear();
        }

        always_lower = conf->GetModule(this)->Get<bool>("always_lower_ts");
    }

    void OnBotDelete(BotInfo *bi) anope_override {
        if (bi == ChanServ) {
            ChanServ = NULL;
        }
    }

    EventReturn OnBotPrivmsg(User *u, BotInfo *bi,
                             Anope::string &message) anope_override {
        if (bi == ChanServ && Config->GetModule(this)->Get<bool>("opersonly") && !u->HasMode("OPER")) {
            u->SendMessage(bi, ACCESS_DENIED);
            return EVENT_STOP;
        }

        return EVENT_CONTINUE;
    }

    void OnDelCore(NickCore *nc) anope_override {
        std::deque<ChannelInfo *> chans;
        nc->GetChannelReferences(chans);
        int max_reg = Config->GetModule(this)->Get<int>("maxregistered");

        for (unsigned i = 0; i < chans.size(); ++i) {
            ChannelInfo *ci = chans[i];

            if (ci->GetFounder() == nc) {
                NickCore *newowner = NULL;
                if (ci->GetSuccessor() && ci->GetSuccessor() != nc
                        && (ci->GetSuccessor()->IsServicesOper() || !max_reg
                            || ci->GetSuccessor()->channelcount < max_reg)) {
                    newowner = ci->GetSuccessor();
                } else {
                    const ChanAccess *highest = NULL;
                    for (unsigned j = 0; j < ci->GetAccessCount(); ++j) {
                        const ChanAccess *ca = ci->GetAccess(j);
                        NickCore *anc = ca->GetAccount();

                        if (!anc || (!anc->IsServicesOper() && max_reg && anc->channelcount >= max_reg)
                                || (anc == nc)) {
                            continue;
                        }
                        if (!highest || *ca > *highest) {
                            highest = ca;
                        }
                    }
                    if (highest) {
                        newowner = highest->GetAccount();
                    }
                }

                if (newowner) {
                    Log(LOG_NORMAL, "chanserv/drop",
                        ChanServ) << "Transferring foundership of " << ci->name << " from deleted nick "
                                  << nc->display << " to " << newowner->display;
                    ci->SetFounder(newowner);
                    ci->SetSuccessor(NULL);
                } else {
                    Log(LOG_NORMAL, "chanserv/drop",
                        ChanServ) << "Deleting channel " << ci->name << " owned by deleted nick " <<
                                  nc->display;

                    delete ci;
                    continue;
                }
            }

            if (ci->GetSuccessor() == nc) {
                ci->SetSuccessor(NULL);
            }

            for (unsigned j = 0; j < ci->GetAccessCount(); ++j) {
                const ChanAccess *ca = ci->GetAccess(j);

                if (ca->GetAccount() == nc) {
                    delete ci->EraseAccess(j);
                    break;
                }
            }

            for (unsigned j = 0; j < ci->GetAkickCount(); ++j) {
                const AutoKick *akick = ci->GetAkick(j);
                if (akick->nc == nc) {
                    ci->EraseAkick(j);
                    break;
                }
            }
        }
    }

    void OnDelChan(ChannelInfo *ci) anope_override {
        /* remove access entries that are this channel */

        std::deque<Anope::string> chans;
        ci->GetChannelReferences(chans);

        for (unsigned i = 0; i < chans.size(); ++i) {
            ChannelInfo *c = ChannelInfo::Find(chans[i]);
            if (!c) {
                continue;
            }

            for (unsigned j = 0; j < c->GetAccessCount(); ++j) {
                ChanAccess *a = c->GetAccess(j);

                if (a->Mask().equals_ci(ci->name)) {
                    delete a;
                    break;
                }
            }
        }

        if (ci->c) {
            ci->c->RemoveMode(ci->WhoSends(), "REGISTERED", "", false);

            const Anope::string &require = Config->GetModule(
                                               this)->Get<const Anope::string>("require");
            if (!require.empty()) {
                ci->c->SetModes(ci->WhoSends(), false, "-%s", require.c_str());
            }
        }
    }

    EventReturn OnPreHelp(CommandSource &source,
                          const std::vector<Anope::string> &params) anope_override {
        if (!params.empty() || source.c || source.service != *ChanServ) {
            return EVENT_CONTINUE;
        }
        source.Reply(_("\002%s\002 allows you to register and control various\n"
                       "aspects of channels. %s can often prevent\n"
                       "malicious users from \"taking over\" channels by limiting\n"
                       "who is allowed channel operator privileges. Available\n"
                       "commands are listed below; to use them, type\n"
                       "\002%s%s \037command\037\002. For more information on a\n"
                       "specific command, type \002%s%s HELP \037command\037\002.\n"),
                     ChanServ->nick.c_str(), ChanServ->nick.c_str(), Config->StrictPrivmsg.c_str(), ChanServ->nick.c_str(), Config->StrictPrivmsg.c_str(), ChanServ->nick.c_str(), ChanServ->nick.c_str(), source.command.c_str());
        return EVENT_CONTINUE;
    }

    void OnPostHelp(CommandSource &source,
                    const std::vector<Anope::string> &params) anope_override {
        if (!params.empty() || source.c || source.service != *ChanServ) {
            return;
        }
        time_t chanserv_expire = Config->GetModule(this)->Get<time_t>("expire", "14d");
        if (chanserv_expire >= 86400)
            source.Reply(_(" \n"
                           "Note that any channel which is not used for %d days\n"
                           "(i.e. which no user on the channel's access list enters\n"
                           "for that period of time) will be automatically dropped."), chanserv_expire / 86400);
        if (source.IsServicesOper())
            source.Reply(_(" \n"
                           "Services Operators can also, depending on their access drop\n"
                           "any channel, view (and modify) the access, levels and akick\n"
                           "lists and settings for any channel."));
    }

    void OnCheckModes(Reference<Channel> &c) anope_override {
        if (!c) {
            return;
        }

        if (c->ci) {
            c->SetMode(c->ci->WhoSends(), "REGISTERED", "", false);
        } else {
            c->RemoveMode(c->ci->WhoSends(), "REGISTERED", "", false);
        }

        const Anope::string &require = Config->GetModule(this)->Get<const Anope::string>("require");
        if (!require.empty()) {
            if (c->ci) {
                c->SetModes(c->ci->WhoSends(), false, "+%s", require.c_str());
            } else {
                c->SetModes(c->ci->WhoSends(), false, "-%s", require.c_str());
            }
        }
    }

    void OnCreateChan(ChannelInfo *ci) anope_override {
        /* Set default chan flags */
        for (unsigned i = 0; i < defaults.size(); ++i) {
            ci->Extend<bool>(defaults[i].upper());
        }
    }

    EventReturn OnCanSet(User *u, const ChannelMode *cm) anope_override {
        if (Config->GetModule(this)->Get<const Anope::string>("nomlock").find(cm->mchar) != Anope::string::npos
                || Config->GetModule(this)->Get<const Anope::string>("require").find(cm->mchar) != Anope::string::npos) {
            return EVENT_STOP;
        }
        return EVENT_CONTINUE;
    }

    void OnChannelSync(Channel *c) anope_override {
        bool perm = c->HasMode("PERM") || (c->ci && persist && persist->HasExt(c->ci));
        if (!perm && !c->botchannel && (c->users.empty() || (c->users.size() == 1 && c->users.begin()->second->user->server == Me))) {
            this->Hold(c);
        }
    }

    void OnLog(Log *l) anope_override {
        if (l->type == LOG_CHANNEL) {
            l->bi = ChanServ;
        }
    }

    void OnExpireTick() anope_override {
        time_t chanserv_expire = Config->GetModule(this)->Get<time_t>("expire", "14d");

        if (!chanserv_expire || Anope::NoExpire || Anope::ReadOnly) {
            return;
        }

        for (registered_channel_map::const_iterator it = RegisteredChannelList->begin(), it_end = RegisteredChannelList->end(); it != it_end; ) {
            ChannelInfo *ci = it->second;
            ++it;

            bool expire = false;

            if (Anope::CurTime - ci->last_used >= chanserv_expire) {
                if (ci->c) {
                    time_t last_used = ci->last_used;
                    for (Channel::ChanUserList::const_iterator cit = ci->c->users.begin(),
                            cit_end = ci->c->users.end(); cit != cit_end
                            && last_used == ci->last_used; ++cit) {
                        ci->AccessFor(cit->second->user);
                    }
                    expire = last_used == ci->last_used;
                } else {
                    expire = true;
                }
            }

            FOREACH_MOD(OnPreChanExpire, (ci, expire));

            if (expire) {
                Log(LOG_NORMAL, "chanserv/expire",
                    ChanServ) << "Expiring channel " << ci->name << " (founder: " <<
                              (ci->GetFounder() ? ci->GetFounder()->display : "(none)") << ")";
                FOREACH_MOD(OnChanExpire, (ci));
                delete ci;
            }
        }
    }

    EventReturn OnCheckDelete(Channel *c) anope_override {
        /* Do not delete this channel if ChanServ/a BotServ bot is inhabiting it */
        if (inhabit.HasExt(c)) {
            return EVENT_STOP;
        }

        return EVENT_CONTINUE;
    }

    void OnPostInit() anope_override {
        if (!persist) {
            return;
        }

        ChannelMode *perm = ModeManager::FindChannelModeByName("PERM");

        /* Find all persistent channels and create them, as we are about to finish burst to our uplink */
        for (registered_channel_map::iterator it = RegisteredChannelList->begin(), it_end = RegisteredChannelList->end(); it != it_end; ++it) {
            ChannelInfo *ci = it->second;
            if (!persist->HasExt(ci)) {
                continue;
            }

            bool c;
            ci->c = Channel::FindOrCreate(ci->name, c, ci->time_registered);
            ci->c->syncing |= created;

            if (perm) {
                ci->c->SetMode(NULL, perm);
            } else {
                if (!ci->bi) {
                    ci->WhoSends()->Assign(NULL, ci);
                }
                if (ci->c->FindUser(ci->bi) == NULL) {
                    ChannelStatus status(BotModes());
                    ci->bi->Join(ci->c, &status);
                }
            }
        }

    }

    void OnChanRegistered(ChannelInfo *ci) anope_override {
        if (!persist || !ci->c) {
            return;
        }
        /* Mark the channel as persistent */
        if (ci->c->HasMode("PERM")) {
            persist->Set(ci);
        }
        /* Persist may be in def cflags, set it here */
        else if (persist->HasExt(ci)) {
            ci->c->SetMode(NULL, "PERM");
        }
    }

    void OnJoinChannel(User *u, Channel *c) anope_override {
        if (always_lower && c->ci && c->creation_time > c->ci->time_registered) {
            Log(LOG_DEBUG) << "Changing TS of " << c->name << " from " << c->creation_time
                           << " to " << c->ci->time_registered;
            c->creation_time = c->ci->time_registered;
            IRCD->SendChannel(c);
            c->Reset();
        }
    }

    EventReturn OnChannelModeSet(Channel *c, MessageSource &setter,
                                 ChannelMode *mode, const Anope::string &param) anope_override {
        if (!always_lower && Anope::CurTime == c->creation_time && c->ci && setter.GetUser() && !setter.GetUser()->server->IsULined()) {
            ChanUserContainer *cu = c->FindUser(setter.GetUser());
            ChannelMode *cm = ModeManager::FindChannelModeByName("OP");
            if (cu && cm && !cu->status.HasMode(cm->mchar)) {
                /* Our -o and their mode change crossing, bounce their mode */
                c->RemoveMode(c->ci->WhoSends(), mode, param);
                /* We don't set mlocks until after the join has finished processing, it will stack with this change,
                 * so there isn't much for the user to remove except -nt etc which is likely locked anyway.
                 */
            }
        }

        return EVENT_CONTINUE;
    }

    void OnChanInfo(CommandSource &source, ChannelInfo *ci, InfoFormatter &info,
                    bool show_all) anope_override {
        if (!show_all) {
            return;
        }

        time_t chanserv_expire = Config->GetModule(this)->Get<time_t>("expire", "14d");
        if (!ci->HasExt("CS_NO_EXPIRE") && chanserv_expire && !Anope::NoExpire && ci->last_used != Anope::CurTime) {
            info[_("Expires")] = Anope::strftime(ci->last_used + chanserv_expire,
                                                 source.GetAccount());
        }
    }

    void OnSetCorrectModes(User *user, Channel *chan, AccessGroup &access,
                           bool &give_modes, bool &take_modes) anope_override {
        if (always_lower)
            // Since we always lower the TS, the other side will remove the modes if the channel ts lowers, so we don't
            // have to worry about it
        {
            take_modes = false;
        } else if (ModeManager::FindChannelModeByName("REGISTERED"))
            // Otherwise if the registered channel mode exists, we should remove modes if the channel is not +r
        {
            take_modes = !chan->HasMode("REGISTERED");
        }
    }
};

MODULE_INIT(ChanServCore)
