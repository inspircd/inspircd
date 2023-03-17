/* Fantasy functionality
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

class CommandBSSetFantasy : public Command {
  public:
    CommandBSSetFantasy(Module *creator,
                        const Anope::string &sname = "botserv/set/fantasy") : Command(creator, sname, 2,
                                    2) {
        this->SetDesc(_("Enable fantaisist commands"));
        this->SetSyntax(_("\037channel\037 {\037ON|OFF\037}"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        const Anope::string &value = params[1];

        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        if (!source.HasPriv("botserv/administration") && !source.AccessFor(ci).HasPriv("SET")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        if (Anope::ReadOnly) {
            source.Reply(_("Sorry, bot option setting is temporarily disabled."));
            return;
        }

        if (value.equals_ci("ON")) {
            bool override = !source.AccessFor(ci).HasPriv("SET");
            Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this,
                ci) << "to enable fantasy";

            ci->Extend<bool>("BS_FANTASY");
            source.Reply(_("Fantasy mode is now \002on\002 on channel %s."),
                         ci->name.c_str());
        } else if (value.equals_ci("OFF")) {
            bool override = !source.AccessFor(ci).HasPriv("SET");
            Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this,
                ci) << "to disable fantasy";

            ci->Shrink<bool>("BS_FANTASY");
            source.Reply(_("Fantasy mode is now \002off\002 on channel %s."),
                         ci->name.c_str());
        } else {
            this->OnSyntaxError(source, source.command);
        }
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(_(" \n"
                       "Enables or disables \002fantasy\002 mode on a channel.\n"
                       "When it is enabled, users will be able to use\n"
                       "fantasy commands on a channel when prefixed\n"
                       "with one of the following fantasy characters: \002%s\002\n"
                       " \n"
                       "Note that users wanting to use fantaisist\n"
                       "commands MUST have enough access for both\n"
                       "the FANTASIA and the command they are executing."),
                     Config->GetModule(this->owner)->Get<const Anope::string>("fantasycharacter", "!").c_str());
        return true;
    }
};

class Fantasy : public Module {
    SerializableExtensibleItem<bool> fantasy;

    CommandBSSetFantasy commandbssetfantasy;

  public:
    Fantasy(const Anope::string &modname,
            const Anope::string &creator) : Module(modname, creator, VENDOR),
        fantasy(this, "BS_FANTASY"), commandbssetfantasy(this) {
    }

    void OnPrivmsg(User *u, Channel *c, Anope::string &msg) anope_override {
        if (!u || !c || !c->ci || !c->ci->bi || msg.empty() || msg[0] == '\1') {
            return;
        }

        if (Config->GetClient("BotServ") && !fantasy.HasExt(c->ci)) {
            return;
        }

        std::vector<Anope::string> params;
        spacesepstream(msg).GetTokens(params);

        if (params.empty()) {
            return;
        }

        Anope::string normalized_param0 = Anope::NormalizeBuffer(params[0]);
        Anope::string fantasy_chars = Config->GetModule(this)->Get<Anope::string>("fantasycharacter", "!");

        if (!normalized_param0.find(c->ci->bi->nick)) {
            params.erase(params.begin());
        } else if (!normalized_param0.find_first_of(fantasy_chars)) {
            size_t sz = params[0].find_first_of(fantasy_chars);
            if (sz == Anope::string::npos) {
                return;    /* normalized_param0 is a subset of params[0] so this can't happen */
            }

            params[0].erase(0, sz + 1);
        } else {
            return;
        }

        if (params.empty()) {
            return;
        }

        CommandInfo::map::const_iterator it = Config->Fantasy.end();
        unsigned count = 0;
        for (unsigned max = params.size(); it == Config->Fantasy.end() && max > 0; --max) {
            Anope::string full_command;
            for (unsigned i = 0; i < max; ++i) {
                full_command += " " + params[i];
            }
            full_command.erase(full_command.begin());

            ++count;
            it = Config->Fantasy.find(Anope::NormalizeBuffer(full_command));
        }

        if (it == Config->Fantasy.end()) {
            return;
        }

        const CommandInfo &info = it->second;
        ServiceReference<Command> cmd("Command", info.name);
        if (!cmd) {
            Log(LOG_DEBUG) << "Fantasy command " << it->first <<
                           " exists for nonexistent service " << info.name << "!";
            return;
        }

        for (unsigned i = 0, j = params.size() - (count - 1); i < j; ++i) {
            params.erase(params.begin());
        }

        /* Some commands take the channel as a first parameter */
        if (info.prepend_channel) {
            params.insert(params.begin(), c->name);
        }

        while (cmd->max_params > 0 && params.size() > cmd->max_params) {
            params[cmd->max_params - 1] += " " + params[cmd->max_params];
            params.erase(params.begin() + cmd->max_params);
        }

        // Command requires registered users only
        if (!cmd->AllowUnregistered() && !u->Account()) {
            return;
        }

        if (params.size() < cmd->min_params) {
            return;
        }

        CommandSource source(u->nick, u, u->Account(), u, c->ci->bi);
        source.c = c;
        source.command = it->first;
        source.permission = info.permission;

        AccessGroup ag = c->ci->AccessFor(u);
        bool has_fantasia = ag.HasPriv("FANTASIA") || source.HasPriv("botserv/fantasy");

        EventReturn MOD_RESULT;
        if (has_fantasia) {
            FOREACH_RESULT(OnBotFantasy, MOD_RESULT, (source, cmd, c->ci, params));
        } else {
            FOREACH_RESULT(OnBotNoFantasyAccess, MOD_RESULT, (source, cmd, c->ci, params));
        }

        if (MOD_RESULT == EVENT_STOP || !has_fantasia) {
            return;
        }

        if (MOD_RESULT != EVENT_ALLOW && !info.permission.empty() && !source.HasCommand(info.permission)) {
            return;
        }

        FOREACH_RESULT(OnPreCommand, MOD_RESULT, (source, cmd, params));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        Reference<NickCore> nc_reference(u->Account());
        cmd->Execute(source, params);
        if (!nc_reference) {
            source.nc = NULL;
        }
        FOREACH_MOD(OnPostCommand, (source, cmd, params));
    }

    void OnBotInfo(CommandSource &source, BotInfo *bi, ChannelInfo *ci,
                   InfoFormatter &info) anope_override {
        if (fantasy.HasExt(ci)) {
            info.AddOption(_("Fantasy"));
        }
    }
};

MODULE_INIT(Fantasy)
