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

class CommandCSSet : public Command {
  public:
    CommandCSSet(Module *creator) : Command(creator, "chanserv/set", 2, 3) {
        this->SetDesc(_("Set channel options and information"));
        this->SetSyntax(_("\037option\037 \037channel\037 \037parameters\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        this->OnSyntaxError(source, "");
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Allows the channel founder to set various channel options\n"
                       "and other information.\n"
                       " \n"
                       "Available options:"));
        Anope::string this_name = source.command;
        bool hide_privileged_commands = Config->GetBlock("options")->Get<bool>("hideprivilegedcommands"),
        hide_registered_commands = Config->GetBlock("options")->Get<bool>("hideregisteredcommands");
        for (CommandInfo::map::const_iterator it = source.service->commands.begin(), it_end = source.service->commands.end(); it != it_end; ++it) {
            const Anope::string &c_name = it->first;
            const CommandInfo &info = it->second;
            if (c_name.find_ci(this_name + " ") == 0) {
                if (info.hide) {
                    continue;
                }

                ServiceReference<Command> c("Command", info.name);

                // XXX dup
                if (!c) {
                    continue;
                } else if (hide_registered_commands && !c->AllowUnregistered()
                           && !source.GetAccount()) {
                    continue;
                } else if (hide_privileged_commands && !info.permission.empty()
                           && !source.HasCommand(info.permission)) {
                    continue;
                }

                source.command = it->first;
                c->OnServHelp(source);
            }
        }
        source.Reply(_("Type \002%s%s HELP %s \037option\037\002 for more information on a\n"
                       "particular option."), Config->StrictPrivmsg.c_str(), source.service->nick.c_str(), this_name.c_str());
        return true;
    }
};

class CommandCSSetAutoOp : public Command {
  public:
    CommandCSSetAutoOp(Module *creator,
                       const Anope::string &cname = "chanserv/set/autoop") : Command(creator, cname, 2,
                                   2) {
        this->SetDesc(_("Should services automatically give status to users"));
        this->SetSyntax(_("\037channel\037 {ON | OFF}"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetChannelOption, MOD_RESULT, (source, this, ci, params[1]));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        if (MOD_RESULT != EVENT_ALLOW && !source.AccessFor(ci).HasPriv("SET") && source.permission.empty() && !source.HasPriv("chanserv/administration")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        if (params[1].equals_ci("ON")) {
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to enable autoop";
            ci->Shrink<bool>("NOAUTOOP");
            source.Reply(
                _("Services will now automatically give modes to users in \002%s\002."),
                ci->name.c_str());
        } else if (params[1].equals_ci("OFF")) {
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to disable autoop";
            ci->Extend<bool>("NOAUTOOP");
            source.Reply(
                _("Services will no longer automatically give modes to users in \002%s\002."),
                ci->name.c_str());
        } else {
            this->OnSyntaxError(source, "AUTOOP");
        }
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Enables or disables %s's autoop feature for a\n"
                       "channel. When disabled, users who join the channel will\n"
                       "not automatically gain any status from %s."), source.service->nick.c_str(),
                     source.service->nick.c_str(), this->name.c_str());
        return true;
    }
};

class CommandCSSetBanType : public Command {
  public:
    CommandCSSetBanType(Module *creator,
                        const Anope::string &cname = "chanserv/set/bantype") : Command(creator, cname,
                                    2, 2) {
        this->SetDesc(_("Set how Services make bans on the channel"));
        this->SetSyntax(_("\037channel\037 \037bantype\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetChannelOption, MOD_RESULT, (source, this, ci, params[1]));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        if (MOD_RESULT != EVENT_ALLOW && !source.AccessFor(ci).HasPriv("SET") && source.permission.empty() && !source.HasPriv("chanserv/administration")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        try {
            int16_t new_type = convertTo<int16_t>(params[1]);
            if (new_type < 0 || new_type > 3) {
                throw ConvertException("Invalid range");
            }
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to change the ban type to " << new_type;
            ci->bantype = new_type;
            source.Reply(_("Ban type for channel %s is now #%d."), ci->name.c_str(),
                         ci->bantype);
        } catch (const ConvertException &) {
            source.Reply(_("\002%s\002 is not a valid ban type."), params[1].c_str());
        }
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Sets the ban type that will be used by services whenever\n"
                       "they need to ban someone from your channel.\n"
                       " \n"
                       "Bantype is a number between 0 and 3 that means:\n"
                       " \n"
                       "0: ban in the form *!user@host\n"
                       "1: ban in the form *!*user@host\n"
                       "2: ban in the form *!*@host\n"
                       "3: ban in the form *!*user@*.domain"), this->name.c_str());
        return true;
    }
};

class CommandCSSetDescription : public Command {
  public:
    CommandCSSetDescription(Module *creator,
                            const Anope::string &cname = "chanserv/set/description") : Command(creator,
                                        cname, 1, 2) {
        this->SetDesc(_("Set the channel description"));
        this->SetSyntax(_("\037channel\037 [\037description\037]"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        const Anope::string &param = params.size() > 1 ? params[1] : "";
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetChannelOption, MOD_RESULT, (source, this, ci, param));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        if (MOD_RESULT != EVENT_ALLOW && !source.AccessFor(ci).HasPriv("SET") && source.permission.empty() && !source.HasPriv("chanserv/administration")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        if (!param.empty()) {
            ci->desc = param;
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to change the description to " << ci->desc;
            source.Reply(_("Description of %s changed to \002%s\002."), ci->name.c_str(),
                         ci->desc.c_str());
        } else {
            ci->desc.clear();
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to unset the description";
            source.Reply(_("Description of %s unset."), ci->name.c_str());
        }

        return;
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Sets the description for the channel, which shows up with\n"
                       "the \002LIST\002 and \002INFO\002 commands."), this->name.c_str());
        return true;
    }
};

class CommandCSSetFounder : public Command {
  public:
    CommandCSSetFounder(Module *creator,
                        const Anope::string &cname = "chanserv/set/founder") : Command(creator, cname,
                                    2, 2) {
        this->SetDesc(_("Set the founder of a channel"));
        this->SetSyntax(_("\037channel\037 \037nick\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetChannelOption, MOD_RESULT, (source, this, ci, params[1]));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        if (MOD_RESULT != EVENT_ALLOW && (ci->HasExt("SECUREFOUNDER") ? !source.IsFounder(ci) : !source.AccessFor(ci).HasPriv("FOUNDER")) && source.permission.empty() && !source.HasPriv("chanserv/administration")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        const NickAlias *na = NickAlias::Find(params[1]);
        if (!na) {
            source.Reply(NICK_X_NOT_REGISTERED, params[1].c_str());
            return;
        }

        NickCore *nc = na->nc;
        unsigned max_reg = Config->GetModule("chanserv")->Get<unsigned>("maxregistered");
        if (max_reg && nc->channelcount >= max_reg && !source.HasPriv("chanserv/no-register-limit")) {
            source.Reply(_("\002%s\002 has too many channels registered."),
                         na->nick.c_str());
            return;
        }

        Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source, this, ci) << "to change the founder from " << (ci->GetFounder() ? ci->GetFounder()->display : "(none)") << " to " << nc->display;

        ci->SetFounder(nc);

        source.Reply(_("Founder of \002%s\002 changed to \002%s\002."), ci->name.c_str(), na->nick.c_str());

        return;
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Changes the founder of a channel. The new nickname must\n"
                       "be a registered one."), this->name.c_str());
        return true;
    }
};

class CommandCSSetKeepModes : public Command {
  public:
    CommandCSSetKeepModes(Module *creator,
                          const Anope::string &cname = "chanserv/set/keepmodes") :  Command(creator,
                                      cname, 2, 2) {
        this->SetDesc(_("Retain modes when channel is not in use"));
        this->SetSyntax(_("\037channel\037 {ON | OFF}"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetChannelOption, MOD_RESULT, (source, this, ci, params[1]));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        if (MOD_RESULT != EVENT_ALLOW && !source.AccessFor(ci).HasPriv("SET") && source.permission.empty() && !source.HasPriv("chanserv/administration")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        if (params[1].equals_ci("ON")) {
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to enable keep modes";
            ci->Extend<bool>("CS_KEEP_MODES");
            source.Reply(_("Keep modes for %s is now \002on\002."), ci->name.c_str());
            if (ci->c) {
                ci->last_modes = ci->c->GetModes();
            }
        } else if (params[1].equals_ci("OFF")) {
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to disable keep modes";
            ci->Shrink<bool>("CS_KEEP_MODES");
            source.Reply(_("Keep modes for %s is now \002off\002."), ci->name.c_str());
            ci->last_modes.clear();
        } else {
            this->OnSyntaxError(source, "KEEPMODES");
        }
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Enables or disables keepmodes for the given channel. If keep\n"
                       "modes is enabled, services will remember modes set on the channel\n"
                       "and attempt to re-set them the next time the channel is created."));
        return true;
    }
};

class CommandCSSetPeace : public Command {
  public:
    CommandCSSetPeace(Module *creator,
                      const Anope::string &cname = "chanserv/set/peace") : Command(creator, cname, 2,
                                  2) {
        this->SetDesc(_("Regulate the use of critical commands"));
        this->SetSyntax(_("\037channel\037 {ON | OFF}"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetChannelOption, MOD_RESULT, (source, this, ci, params[1]));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        if (MOD_RESULT != EVENT_ALLOW && !source.AccessFor(ci).HasPriv("SET") && source.permission.empty() && !source.HasPriv("chanserv/administration")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        if (params[1].equals_ci("ON")) {
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to enable peace";
            ci->Extend<bool>("PEACE");
            source.Reply(_("Peace option for %s is now \002on\002."), ci->name.c_str());
        } else if (params[1].equals_ci("OFF")) {
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to disable peace";
            ci->Shrink<bool>("PEACE");
            source.Reply(_("Peace option for %s is now \002off\002."), ci->name.c_str());
        } else {
            this->OnSyntaxError(source, "PEACE");
        }

        return;
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Enables or disables the \002peace\002 option for a channel.\n"
                       "When \002peace\002 is set, a user won't be able to kick,\n"
                       "ban or remove a channel status of a user that has\n"
                       "a level superior or equal to his via %s commands."), source.service->nick.c_str());
        return true;
    }
};

inline static Anope::string BotModes() {
    return Config->GetModule("botserv")->Get<Anope::string>("botmodes",
            Config->GetModule("chanserv")->Get<Anope::string>("botmodes", "o")
                                                           );
}

class CommandCSSetPersist : public Command {
  public:
    CommandCSSetPersist(Module *creator,
                        const Anope::string &cname = "chanserv/set/persist") : Command(creator, cname,
                                    2, 2) {
        this->SetDesc(_("Set the channel as permanent"));
        this->SetSyntax(_("\037channel\037 {ON | OFF}"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetChannelOption, MOD_RESULT, (source, this, ci, params[1]));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        if (MOD_RESULT != EVENT_ALLOW && !source.AccessFor(ci).HasPriv("SET") && source.permission.empty() && !source.HasPriv("chanserv/administration")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        ChannelMode *cm = ModeManager::FindChannelModeByName("PERM");

        if (params[1].equals_ci("ON")) {
            if (!ci->HasExt("PERSIST")) {
                ci->Extend<bool>("PERSIST");

                /* Set the perm mode */
                if (cm) {
                    if (ci->c && !ci->c->HasMode("PERM")) {
                        ci->c->SetMode(NULL, cm);
                    }
                    /* Add it to the channels mlock */
                    ModeLocks *ml = ci->Require<ModeLocks>("modelocks");
                    if (ml) {
                        ml->SetMLock(cm, true, "", source.GetNick());
                    }
                }
                /* No botserv bot, no channel mode, give them ChanServ.
                 * Yes, this works fine with no BotServ.
                 */
                else if (!ci->bi) {
                    BotInfo *ChanServ = Config->GetClient("ChanServ");
                    if (!ChanServ) {
                        source.Reply(_("ChanServ is required to enable persist on this network."));
                        return;
                    }

                    ChanServ->Assign(NULL, ci);
                    if (ci->c && !ci->c->FindUser(ChanServ)) {
                        ChannelStatus status(BotModes());
                        ChanServ->Join(ci->c, &status);
                    }
                }
            }

            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to enable persist";
            source.Reply(_("Channel \002%s\002 is now persistent."), ci->name.c_str());
        } else if (params[1].equals_ci("OFF")) {
            if (ci->HasExt("PERSIST")) {
                ci->Shrink<bool>("PERSIST");

                BotInfo *ChanServ = Config->GetClient("ChanServ"),
                         *BotServ = Config->GetClient("BotServ");

                /* Unset perm mode */
                if (cm) {
                    if (ci->c && ci->c->HasMode("PERM")) {
                        ci->c->RemoveMode(NULL, cm);
                    }
                    /* Remove from mlock */
                    ModeLocks *ml = ci->GetExt<ModeLocks>("modelocks");
                    if (ml) {
                        ml->RemoveMLock(cm, true);
                    }
                }
                /* No channel mode, no BotServ, but using ChanServ as the botserv bot
                 * which was assigned when persist was set on
                 */
                else if (!cm && !BotServ && ci->bi) {
                    if (!ChanServ) {
                        source.Reply(_("ChanServ is required to enable persist on this network."));
                        return;
                    }

                    /* Unassign bot */
                    ChanServ->UnAssign(NULL, ci);
                }
            }

            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to disable persist";
            source.Reply(_("Channel \002%s\002 is no longer persistent."),
                         ci->name.c_str());
        } else {
            this->OnSyntaxError(source, "PERSIST");
        }
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        BotInfo *BotServ = Config->GetClient("BotServ");
        BotInfo *ChanServ = Config->GetClient("ChanServ");
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Enables or disables the persistent channel setting.\n"
                       "When persistent is set, the service bot will remain\n"
                       "in the channel when it has emptied of users.\n"
                       " \n"
                       "If your IRCd does not have a permanent (persistent) channel\n"
                       "mode you must have a service bot in your channel to\n"
                       "set persist on, and it can not be unassigned while persist\n"
                       "is on.\n"
                       " \n"
                       "If this network does not have %s enabled and does\n"
                       "not have a permanent channel mode, %s will\n"
                       "join your channel when you set persist on (and leave when\n"
                       "it has been set off).\n"
                       " \n"
                       "If your IRCd has a permanent (persistent) channel mode\n"
                       "and it is set or unset (for any reason, including MODE LOCK),\n"
                       "persist is automatically set and unset for the channel as well.\n"
                       "Additionally, services will set or unset this mode when you\n"
                       "set persist on or off."), BotServ ? BotServ->nick.c_str() : "BotServ",
                     ChanServ ? ChanServ->nick.c_str() : "ChanServ");
        return true;
    }
};

class CommandCSSetRestricted : public Command {
  public:
    CommandCSSetRestricted(Module *creator,
                           const Anope::string &cname = "chanserv/set/restricted") : Command(creator,
                                       cname, 2, 2) {
        this->SetDesc(_("Restrict access to the channel"));
        this->SetSyntax(_("\037channel\037 {ON | OFF}"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetChannelOption, MOD_RESULT, (source, this, ci, params[1]));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        if (MOD_RESULT != EVENT_ALLOW && !source.AccessFor(ci).HasPriv("SET") && source.permission.empty() && !source.HasPriv("chanserv/administration")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        if (params[1].equals_ci("ON")) {
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to enable restricted";
            ci->Extend<bool>("RESTRICTED");
            source.Reply(_("Restricted access option for %s is now \002on\002."),
                         ci->name.c_str());
        } else if (params[1].equals_ci("OFF")) {
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to disable restricted";
            ci->Shrink<bool>("RESTRICTED");
            source.Reply(_("Restricted access option for %s is now \002off\002."),
                         ci->name.c_str());
        } else {
            this->OnSyntaxError(source, "RESTRICTED");
        }
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Enables or disables the \002restricted access\002 option for a\n"
                       "channel. When \002restricted access\002 is set, users not on the access list will\n"
                       "instead be kicked and banned from the channel."));
        return true;
    }
};

class CommandCSSetSecure : public Command {
  public:
    CommandCSSetSecure(Module *creator,
                       const Anope::string &cname = "chanserv/set/secure") : Command(creator, cname, 2,
                                   2) {
        this->SetDesc(_("Activate security features"));
        this->SetSyntax(_("\037channel\037 {ON | OFF}"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetChannelOption, MOD_RESULT, (source, this, ci, params[1]));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        if (MOD_RESULT != EVENT_ALLOW && !source.AccessFor(ci).HasPriv("SET") && source.permission.empty() && !source.HasPriv("chanserv/administration")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        if (params[1].equals_ci("ON")) {
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to enable secure";
            ci->Extend<bool>("CS_SECURE");
            source.Reply(_("Secure option for %s is now \002on\002."), ci->name.c_str());
        } else if (params[1].equals_ci("OFF")) {
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to disable secure";
            ci->Shrink<bool>("CS_SECURE");
            source.Reply(_("Secure option for %s is now \002off\002."), ci->name.c_str());
        } else {
            this->OnSyntaxError(source, "SECURE");
        }
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Enables or disables security features for a\n"
                       "channel. When \002SECURE\002 is set, only users who have\n"
                       "identified to services, and are not only recognized, will be\n"
                       "given access to channels from account-based access entries."));
        return true;
    }
};

class CommandCSSetSecureFounder : public Command {
  public:
    CommandCSSetSecureFounder(Module *creator,
                              const Anope::string &cname = "chanserv/set/securefounder") : Command(creator,
                                          cname, 2, 2) {
        this->SetDesc(_("Stricter control of channel founder status"));
        this->SetSyntax(_("\037channel\037 {ON | OFF}"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetChannelOption, MOD_RESULT, (source, this, ci, params[1]));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        if (MOD_RESULT != EVENT_ALLOW && (ci->HasExt("SECUREFOUNDER") ? !source.IsFounder(ci) : !source.AccessFor(ci).HasPriv("FOUNDER")) && source.permission.empty() && !source.HasPriv("chanserv/administration")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        if (params[1].equals_ci("ON")) {
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to enable secure founder";
            ci->Extend<bool>("SECUREFOUNDER");
            source.Reply(_("Secure founder option for %s is now \002on\002."),
                         ci->name.c_str());
        } else if (params[1].equals_ci("OFF")) {
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to disable secure founder";
            ci->Shrink<bool>("SECUREFOUNDER");
            source.Reply(_("Secure founder option for %s is now \002off\002."),
                         ci->name.c_str());
        } else {
            this->OnSyntaxError(source, "SECUREFOUNDER");
        }
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Enables or disables the \002secure founder\002 option for a channel.\n"
                       "When \002secure founder\002 is set, only the real founder will be\n"
                       "able to drop the channel, change its founder and its successor,\n"
                       "and not those who have founder level access through\n"
                       "the access/qop command."));
        return true;
    }
};

class CommandCSSetSecureOps : public Command {
  public:
    CommandCSSetSecureOps(Module *creator,
                          const Anope::string &cname = "chanserv/set/secureops") : Command(creator, cname,
                                      2, 2) {
        this->SetDesc(_("Stricter control of chanop status"));
        this->SetSyntax(_("\037channel\037 {ON | OFF}"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetChannelOption, MOD_RESULT, (source, this, ci, params[1]));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        if (MOD_RESULT != EVENT_ALLOW && !source.AccessFor(ci).HasPriv("SET") && source.permission.empty() && !source.HasPriv("chanserv/administration")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        if (params[1].equals_ci("ON")) {
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to enable secure ops";
            ci->Extend<bool>("SECUREOPS");
            source.Reply(_("Secure ops option for %s is now \002on\002."),
                         ci->name.c_str());
        } else if (params[1].equals_ci("OFF")) {
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to disable secure ops";
            ci->Shrink<bool>("SECUREOPS");
            source.Reply(_("Secure ops option for %s is now \002off\002."),
                         ci->name.c_str());
        } else {
            this->OnSyntaxError(source, "SECUREOPS");
        }
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Enables or disables the \002secure ops\002 option for a channel.\n"
                       "When \002secure ops\002 is set, users who are not on the access list\n"
                       "will not be allowed channel operator status."));
        return true;
    }
};

class CommandCSSetSignKick : public Command {
  public:
    CommandCSSetSignKick(Module *creator,
                         const Anope::string &cname = "chanserv/set/signkick") : Command(creator, cname,
                                     2, 2) {
        this->SetDesc(_("Sign kicks that are done with the KICK command"));
        this->SetSyntax(_("\037channel\037 {ON | LEVEL | OFF}"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetChannelOption, MOD_RESULT, (source, this, ci, params[1]));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        if (MOD_RESULT != EVENT_ALLOW && !source.AccessFor(ci).HasPriv("SET") && source.permission.empty() && !source.HasPriv("chanserv/administration")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        if (params[1].equals_ci("ON")) {
            ci->Extend<bool>("SIGNKICK");
            ci->Shrink<bool>("SIGNKICK_LEVEL");
            source.Reply(_("Signed kick option for %s is now \002on\002."),
                         ci->name.c_str());
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to enable sign kick";
        } else if (params[1].equals_ci("LEVEL")) {
            ci->Extend<bool>("SIGNKICK_LEVEL");
            ci->Shrink<bool>("SIGNKICK");
            source.Reply(
                _("Signed kick option for %s is now \002on\002, but depends of the\n"
                  "level of the user that is using the command."), ci->name.c_str());
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to enable sign kick level";
        } else if (params[1].equals_ci("OFF")) {
            ci->Shrink<bool>("SIGNKICK");
            ci->Shrink<bool>("SIGNKICK_LEVEL");
            source.Reply(_("Signed kick option for %s is now \002off\002."),
                         ci->name.c_str());
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to disable sign kick";
        } else {
            this->OnSyntaxError(source, "SIGNKICK");
        }
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Enables or disables signed kicks for a\n"
                       "channel.  When \002SIGNKICK\002 is set, kicks issued with\n"
                       "the \002KICK\002 command will have the nick that used the\n"
                       "command in their reason.\n"
                       " \n"
                       "If you use \002LEVEL\002, those who have a level that is superior\n"
                       "or equal to the SIGNKICK level on the channel won't have their\n"
                       "kicks signed."));
        return true;
    }
};

class CommandCSSetSuccessor : public Command {
  public:
    CommandCSSetSuccessor(Module *creator,
                          const Anope::string &cname = "chanserv/set/successor") : Command(creator, cname,
                                      1, 2) {
        this->SetDesc(_("Set the successor for a channel"));
        this->SetSyntax(_("\037channel\037 [\037nick\037]"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        const Anope::string &param = params.size() > 1 ? params[1] : "";
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetChannelOption, MOD_RESULT, (source, this, ci, param));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        if (MOD_RESULT != EVENT_ALLOW && (ci->HasExt("SECUREFOUNDER") ? !source.IsFounder(ci) : !source.AccessFor(ci).HasPriv("FOUNDER")) && source.permission.empty() && !source.HasPriv("chanserv/administration")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        NickCore *nc;

        if (!param.empty()) {
            const NickAlias *na = NickAlias::Find(param);

            if (!na) {
                source.Reply(NICK_X_NOT_REGISTERED, param.c_str());
                return;
            }
            if (na->nc == ci->GetFounder()) {
                source.Reply(
                    _("%s cannot be the successor on channel %s as they are the founder."),
                    na->nick.c_str(), ci->name.c_str());
                return;
            }
            nc = na->nc;
        } else {
            nc = NULL;
        }

        Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source, this, ci) << "to change the successor from " << (ci->GetSuccessor() ? ci->GetSuccessor()->display : "(none)") << " to " << (nc ? nc->display : "(none)");

        ci->SetSuccessor(nc);

        if (nc) {
            source.Reply(_("Successor for \002%s\002 changed to \002%s\002."),
                         ci->name.c_str(), nc->display.c_str());
        } else {
            source.Reply(_("Successor for \002%s\002 unset."), ci->name.c_str());
        }

        return;
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Changes the successor of a channel. If the founder's\n"
                       "nickname expires or is dropped while the channel is still\n"
                       "registered, the successor will become the new founder of the\n"
                       "channel. The successor's nickname must be a registered one.\n"
                       "If there's no successor set, then the first nickname on the\n"
                       "access list (with the highest access, if applicable) will\n"
                       "become the new founder, but if the access list is empty, the\n"
                       "channel will be dropped."));
        unsigned max_reg = Config->GetModule("chanserv")->Get<unsigned>("maxregistered");
        if (max_reg) {
            source.Reply(" ");
            source.Reply(_("Note, however, if the successor already has too many\n"
                           "channels registered (%d), they will not be able to\n"
                           "become the new founder and it will be as if the\n"
                           "channel had no successor set."), max_reg);
        }
        return true;
    }
};

class CommandCSSetNoexpire : public Command {
  public:
    CommandCSSetNoexpire(Module *creator) : Command(creator,
                "chanserv/saset/noexpire", 2, 2) {
        this->SetDesc(_("Prevent the channel from expiring"));
        this->SetSyntax(_("\037channel\037 {ON | OFF}"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        if (source.permission.empty() && !source.AccessFor(ci).HasPriv("SET")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        if (params[1].equals_ci("ON")) {
            Log(LOG_ADMIN, source, this, ci) << "to enable noexpire";
            ci->Extend<bool>("CS_NO_EXPIRE");
            source.Reply(_("Channel %s \002will not\002 expire."), ci->name.c_str());
        } else if (params[1].equals_ci("OFF")) {
            Log(LOG_ADMIN, source, this, ci) << "to disable noexpire";
            ci->Shrink<bool>("CS_NO_EXPIRE");
            source.Reply(_("Channel %s \002will\002 expire."), ci->name.c_str());
        } else {
            this->OnSyntaxError(source, "NOEXPIRE");
        }

        return;
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Sets whether the given channel will expire.  Setting this\n"
                       "to ON prevents the channel from expiring."));
        return true;
    }
};

class CSSet : public Module {
    SerializableExtensibleItem<bool> noautoop, peace, securefounder,
                               restricted, secure, secureops, signkick, signkick_level, noexpire,
                               persist;

    struct KeepModes : SerializableExtensibleItem<bool> {
        KeepModes(Module *m, const Anope::string &n) : SerializableExtensibleItem<bool>
            (m, n) { }

        void ExtensibleSerialize(const Extensible *e, const Serializable *s,
                                 Serialize::Data &data) const anope_override {
            SerializableExtensibleItem<bool>::ExtensibleSerialize(e, s, data);

            if (s->GetSerializableType()->GetName() != "ChannelInfo") {
                return;
            }

            const ChannelInfo *ci = anope_dynamic_static_cast<const ChannelInfo *>(s);
            Anope::string modes;
            for (Channel::ModeList::const_iterator it = ci->last_modes.begin();
                    it != ci->last_modes.end(); ++it) {
                if (!modes.empty()) {
                    modes += " ";
                }
                modes += it->first;
                if (!it->second.empty()) {
                    modes += "," + it->second;
                }
            }
            data["last_modes"] << modes;
        }

        void ExtensibleUnserialize(Extensible *e, Serializable *s,
                                   Serialize::Data &data) anope_override {
            SerializableExtensibleItem<bool>::ExtensibleUnserialize(e, s, data);

            if (s->GetSerializableType()->GetName() != "ChannelInfo") {
                return;
            }

            ChannelInfo *ci = anope_dynamic_static_cast<ChannelInfo *>(s);
            Anope::string modes;
            data["last_modes"] >> modes;
            ci->last_modes.clear();
            for (spacesepstream sep(modes); sep.GetToken(modes);) {
                size_t c = modes.find(',');
                if (c == Anope::string::npos) {
                    ci->last_modes.insert(std::make_pair(modes, ""));
                } else {
                    ci->last_modes.insert(std::make_pair(modes.substr(0, c), modes.substr(c + 1)));
                }
            }
        }
    } keep_modes;

    CommandCSSet commandcsset;
    CommandCSSetAutoOp commandcssetautoop;
    CommandCSSetBanType commandcssetbantype;
    CommandCSSetDescription commandcssetdescription;
    CommandCSSetFounder commandcssetfounder;
    CommandCSSetKeepModes commandcssetkeepmodes;
    CommandCSSetPeace commandcssetpeace;
    CommandCSSetPersist commandcssetpersist;
    CommandCSSetRestricted commandcssetrestricted;
    CommandCSSetSecure commandcssetsecure;
    CommandCSSetSecureFounder commandcssetsecurefounder;
    CommandCSSetSecureOps commandcssetsecureops;
    CommandCSSetSignKick commandcssetsignkick;
    CommandCSSetSuccessor commandcssetsuccessor;
    CommandCSSetNoexpire commandcssetnoexpire;

    ExtensibleRef<bool> inhabit;

    bool persist_lower_ts;

  public:
    CSSet(const Anope::string &modname,
          const Anope::string &creator) : Module(modname, creator, VENDOR),
        noautoop(this, "NOAUTOOP"), peace(this, "PEACE"),
        securefounder(this, "SECUREFOUNDER"), restricted(this, "RESTRICTED"),
        secure(this, "CS_SECURE"), secureops(this, "SECUREOPS"), signkick(this,
                "SIGNKICK"),
        signkick_level(this, "SIGNKICK_LEVEL"), noexpire(this, "CS_NO_EXPIRE"),
        persist(this, "PERSIST"),
        keep_modes(this, "CS_KEEP_MODES"),

        commandcsset(this), commandcssetautoop(this), commandcssetbantype(this),
        commandcssetdescription(this), commandcssetfounder(this),
        commandcssetkeepmodes(this),
        commandcssetpeace(this), commandcssetpersist(this),
        commandcssetrestricted(this),
        commandcssetsecure(this), commandcssetsecurefounder(this),
        commandcssetsecureops(this), commandcssetsignkick(this),
        commandcssetsuccessor(this), commandcssetnoexpire(this),

        inhabit("inhabit") {
    }

    void OnReload(Configuration::Conf *conf) anope_override {
        persist_lower_ts = conf->GetModule(this)->Get<bool>("persist_lower_ts");
    }

    void OnCreateChan(ChannelInfo *ci) anope_override {
        ci->bantype = Config->GetModule(this)->Get<int>("defbantype", "2");
    }

    void OnChannelSync(Channel *c) anope_override {
        if (c->ci && keep_modes.HasExt(c->ci)) {
            Channel::ModeList ml = c->ci->last_modes;
            for (Channel::ModeList::iterator it = ml.begin(); it != ml.end(); ++it) {
                c->SetMode(c->ci->WhoSends(), it->first, it->second);
            }
        }
    }

    EventReturn OnCheckKick(User *u, Channel *c, Anope::string &mask,
                            Anope::string &reason) anope_override {
        if (!c->ci || !restricted.HasExt(c->ci) || c->MatchesList(u, "EXCEPT")) {
            return EVENT_CONTINUE;
        }

        if (c->ci->AccessFor(u).empty() && (!c->ci->GetFounder() || u->Account() != c->ci->GetFounder())) {
            return EVENT_STOP;
        }

        return EVENT_CONTINUE;
    }

    void OnDelChan(ChannelInfo *ci) anope_override {
        if (ci->c && persist.HasExt(ci)) {
            ci->c->RemoveMode(ci->WhoSends(), "PERM", "", false);
        }
        persist.Unset(ci);
    }

    EventReturn OnChannelModeSet(Channel *c, MessageSource &setter,
                                 ChannelMode *mode, const Anope::string &param) anope_override {
        if (c->ci) {
            /* Channel mode +P or so was set, mark this channel as persistent */
            if (mode->name == "PERM") {
                persist.Set(c->ci, true);
            }

            if (mode->type != MODE_STATUS && !c->syncing && Me->IsSynced() && (!inhabit
                    || !inhabit->HasExt(c))) {
                c->ci->last_modes = c->GetModes();
            }
        }

        return EVENT_CONTINUE;
    }

    EventReturn OnChannelModeUnset(Channel *c, MessageSource &setter,
                                   ChannelMode *mode, const Anope::string &param) anope_override {
        if (mode->name == "PERM") {
            if (c->ci) {
                persist.Unset(c->ci);
            }
        }

        if (c->ci && mode->type != MODE_STATUS && !c->syncing && Me->IsSynced() && (!inhabit || !inhabit->HasExt(c))) {
            c->ci->last_modes = c->GetModes();
        }

        return EVENT_CONTINUE;
    }

    void OnJoinChannel(User *u, Channel *c) anope_override {
        if (u->server != Me && persist_lower_ts && c->ci && persist.HasExt(c->ci) && c->creation_time > c->ci->time_registered) {
            Log(LOG_DEBUG) << "Changing TS of " << c->name << " from " << c->creation_time
                           << " to " << c->ci->time_registered;
            c->creation_time = c->ci->time_registered;
            IRCD->SendChannel(c);
            c->Reset();
        }
    }

    void OnSetCorrectModes(User *user, Channel *chan, AccessGroup &access,
                           bool &give_modes, bool &take_modes) anope_override {
        if (chan->ci) {
            if (noautoop.HasExt(chan->ci)) {
                give_modes = false;
            }
            if (secureops.HasExt(chan->ci) && !user->HasPriv("chanserv/administration"))
                // This overrides what chanserv does because it is loaded after chanserv
            {
                take_modes = true;
            }
        }
    }

    void OnPreChanExpire(ChannelInfo *ci, bool &expire) anope_override {
        if (noexpire.HasExt(ci)) {
            expire = false;
        }
    }

    void OnChanInfo(CommandSource &source, ChannelInfo *ci, InfoFormatter &info,
                    bool show_all) anope_override {
        if (!show_all) {
            return;
        }

        if (peace.HasExt(ci)) {
            info.AddOption(_("Peace"));
        }
        if (restricted.HasExt(ci)) {
            info.AddOption(_("Restricted access"));
        }
        if (secure.HasExt(ci)) {
            info.AddOption(_("Security"));
        }
        if (securefounder.HasExt(ci)) {
            info.AddOption(_("Secure founder"));
        }
        if (secureops.HasExt(ci)) {
            info.AddOption(_("Secure ops"));
        }
        if (signkick.HasExt(ci) || signkick_level.HasExt(ci)) {
            info.AddOption(_("Signed kicks"));
        }
        if (persist.HasExt(ci)) {
            info.AddOption(_("Persistent"));
        }
        if (noexpire.HasExt(ci)) {
            info.AddOption(_("No expire"));
        }
        if (keep_modes.HasExt(ci)) {
            info.AddOption(_("Keep modes"));
        }
        if (noautoop.HasExt(ci)) {
            info.AddOption(_("No auto-op"));
        }
    }
};

MODULE_INIT(CSSet)
