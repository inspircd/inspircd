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

class CommandBSSet : public Command {
  public:
    CommandBSSet(Module *creator) : Command(creator, "botserv/set", 3, 3) {
        this->SetDesc(_("Configures bot options"));
        this->SetSyntax(_("\037option\037 \037(channel | bot)\037 \037settings\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        this->OnSyntaxError(source, "");
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Configures bot options.\n"
                       " \n"
                       "Available options:"));
        bool hide_privileged_commands = Config->GetBlock("options")->Get<bool>("hideprivilegedcommands"),
        hide_registered_commands = Config->GetBlock("options")->Get<bool>("hideregisteredcommands");
        Anope::string this_name = source.command;
        for (CommandInfo::map::const_iterator it = source.service->commands.begin(), it_end = source.service->commands.end(); it != it_end; ++it) {
            const Anope::string &c_name = it->first;
            const CommandInfo &info = it->second;
            if (c_name.find_ci(this_name + " ") == 0) {
                if (info.hide) {
                    continue;
                }

                ServiceReference<Command> command("Command", info.name);
                if (command) {
                    // XXX dup
                    if (hide_registered_commands && !command->AllowUnregistered()
                            && !source.GetAccount()) {
                        continue;
                    }

                    if (hide_privileged_commands && !info.permission.empty()
                            && !source.HasCommand(info.permission)) {
                        continue;
                    }

                    source.command = it->first;
                    command->OnServHelp(source);
                }
            }
        }
        source.Reply(_("Type \002%s%s HELP %s \037option\037\002 for more information on a\n"
                       "particular option."), Config->StrictPrivmsg.c_str(), source.service->nick.c_str(), this_name.c_str());

        return true;
    }
};

class CommandBSSetBanExpire : public Command {
  public:
    class UnbanTimer : public Timer {
        Anope::string chname;
        Anope::string mask;

      public:
        UnbanTimer(Module *creator, const Anope::string &ch, const Anope::string &bmask,
                   time_t t) : Timer(creator, t), chname(ch), mask(bmask) { }

        void Tick(time_t) anope_override {
            Channel *c = Channel::Find(chname);
            if (c) {
                c->RemoveMode(NULL, "BAN", mask);
            }
        }
    };

    CommandBSSetBanExpire(Module *creator,
                          const Anope::string &sname = "botserv/set/banexpire") : Command(creator, sname,
                                      2, 2) {
        this->SetDesc(_("Configures the time bot bans expire in"));
        this->SetSyntax(_("\037channel\037 \037time\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &chan = params[0];
        const Anope::string &arg = params[1];

        ChannelInfo *ci = ChannelInfo::Find(chan);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, chan.c_str());
            return;
        }

        AccessGroup access = source.AccessFor(ci);
        if (!source.HasPriv("botserv/administration") && !access.HasPriv("SET")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        if (Anope::ReadOnly) {
            source.Reply(_("Sorry, changing bot options is temporarily disabled."));
            return;
        }

        time_t t = Anope::DoTime(arg);
        if (t < 0) {
            source.Reply(BAD_EXPIRY_TIME);
            return;
        }

        /* cap at 1 day */
        if (t > 86400) {
            source.Reply(_("Ban expiry may not be longer than 1 day."));
            return;
        }

        ci->banexpire = t;

        bool override = !access.HasPriv("SET");
        Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this, ci) << "to change banexpire to " << ci->banexpire;

        if (!ci->banexpire) {
            source.Reply(_("Bot bans will no longer automatically expire."));
        } else {
            source.Reply(_("Bot bans will automatically expire after %s."),
                         Anope::Duration(ci->banexpire, source.GetAccount()).c_str());
        }
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(_(" \n"
                       "Sets the time bot bans expire in. If enabled, any bans placed by\n"
                       "bots, such as flood kicker, badwords kicker, etc. will automatically\n"
                       "be removed after the given time. Set to 0 to disable bans from\n"
                       "automatically expiring."));
        return true;
    }
};

class CommandBSSetPrivate : public Command {
  public:
    CommandBSSetPrivate(Module *creator,
                        const Anope::string &sname = "botserv/set/private") : Command(creator, sname, 2,
                                    2) {
        this->SetDesc(_("Prevent a bot from being assigned by non IRC operators"));
        this->SetSyntax(_("\037botname\037 {\037ON|OFF\037}"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        BotInfo *bi = BotInfo::Find(params[0], true);
        const Anope::string &value = params[1];

        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        if (bi == NULL) {
            source.Reply(BOT_DOES_NOT_EXIST, params[0].c_str());
            return;
        }

        if (value.equals_ci("ON")) {
            bi->oper_only = true;
            source.Reply(_("Private mode of bot %s is now \002on\002."), bi->nick.c_str());
        } else if (value.equals_ci("OFF")) {
            bi->oper_only = false;
            source.Reply(_("Private mode of bot %s is now \002off\002."), bi->nick.c_str());
        } else {
            this->OnSyntaxError(source, source.command);
        }
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(_(" \n"
                       "This option prevents a bot from being assigned to a\n"
                       "channel by users that aren't IRC Operators."));
        return true;
    }
};

class BSSet : public Module {
    CommandBSSet commandbsset;
    CommandBSSetBanExpire commandbssetbanexpire;
    CommandBSSetPrivate commandbssetprivate;

  public:
    BSSet(const Anope::string &modname,
          const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandbsset(this), commandbssetbanexpire(this),
        commandbssetprivate(this) {
    }

    void OnBotBan(User *u, ChannelInfo *ci,
                  const Anope::string &mask) anope_override {
        if (!ci->banexpire) {
            return;
        }

        new CommandBSSetBanExpire::UnbanTimer(this, ci->name, mask, ci->banexpire);
    }
};

MODULE_INIT(BSSet)
