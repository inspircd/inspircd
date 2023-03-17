/*
 *
 * (C) 2008-2011 Robin Burchell <w00t@inspircd.org>
 * (C) 2008-2023 Anope Team <team@anope.org>
 *
 * Please read COPYING and README for further details.
 */

#include "services.h"
#include "commands.h"
#include "users.h"
#include "language.h"
#include "config.h"
#include "bots.h"
#include "opertype.h"
#include "access.h"
#include "regchannel.h"
#include "channels.h"

CommandSource::CommandSource(const Anope::string &n, User *user, NickCore *core,
                             CommandReply *r, BotInfo *bi) : nick(n), u(user), nc(core), reply(r),
    c(NULL), service(bi) {
}

const Anope::string &CommandSource::GetNick() const {
    return this->nick;
}

User *CommandSource::GetUser() {
    return this->u;
}

NickCore *CommandSource::GetAccount() {
    return this->nc;
}

AccessGroup CommandSource::AccessFor(ChannelInfo *ci) {
    if (this->u) {
        return ci->AccessFor(this->u);
    } else if (this->nc) {
        return ci->AccessFor(this->nc);
    } else {
        return AccessGroup();
    }
}

bool CommandSource::IsFounder(ChannelInfo *ci) {
    if (this->u) {
        return ::IsFounder(this->u, ci);
    } else if (this->nc) {
        return *this->nc == ci->GetFounder();
    }
    return false;
}

bool CommandSource::HasCommand(const Anope::string &cmd) {
    if (this->u) {
        return this->u->HasCommand(cmd);
    } else if (this->nc && this->nc->o) {
        return this->nc->o->ot->HasCommand(cmd);
    }
    return false;
}

bool CommandSource::HasPriv(const Anope::string &cmd) {
    if (this->u) {
        return this->u->HasPriv(cmd);
    } else if (this->nc && this->nc->o) {
        return this->nc->o->ot->HasPriv(cmd);
    }
    return false;
}

bool CommandSource::IsServicesOper() {
    if (this->u) {
        return this->u->IsServicesOper();
    } else if (this->nc) {
        return this->nc->IsServicesOper();
    }
    return false;
}

bool CommandSource::IsOper() {
    if (this->u) {
        return this->u->HasMode("OPER");
    } else if (this->nc) {
        return this->nc->IsServicesOper();
    }
    return false;
}

void CommandSource::Reply(const char *message, ...) {
    va_list args;
    char buf[4096]; // Messages can be really big.

    const char *translated_message = Language::Translate(this->nc, message);

    va_start(args, message);
    vsnprintf(buf, sizeof(buf), translated_message, args);

    this->Reply(Anope::string(buf));

    va_end(args);
}

void CommandSource::Reply(const Anope::string &message) {
    const char *translated_message = Language::Translate(this->nc, message.c_str());

    sepstream sep(translated_message, '\n', true);
    Anope::string tok;
    while (sep.GetToken(tok)) {
        this->reply->SendMessage(this->service, tok);
    }
}

Command::Command(Module *o, const Anope::string &sname, size_t minparams,
                 size_t maxparams) : Service(o, "Command", sname), max_params(maxparams),
    min_params(minparams), module(o) {
    allow_unregistered = require_user = false;
}

Command::~Command() {
}

void Command::SetDesc(const Anope::string &d) {
    this->desc = d;
}

void Command::ClearSyntax() {
    this->syntax.clear();
}

void Command::SetSyntax(const Anope::string &s) {
    this->syntax.push_back(s);
}

void Command::SendSyntax(CommandSource &source) {
    Anope::string s = Language::Translate(source.GetAccount(), _("Syntax"));
    if (!this->syntax.empty()) {
        source.Reply("%s: \002%s %s\002", s.c_str(), source.command.c_str(),
                     Language::Translate(source.GetAccount(), this->syntax[0].c_str()));
        Anope::string spaces(s.length(), ' ');
        for (unsigned i = 1, j = this->syntax.size(); i < j; ++i) {
            source.Reply("%s  \002%s %s\002", spaces.c_str(), source.command.c_str(),
                         Language::Translate(source.GetAccount(), this->syntax[i].c_str()));
        }
    } else {
        source.Reply("%s: \002%s\002", s.c_str(), source.command.c_str());
    }
}

bool Command::AllowUnregistered() const {
    return this->allow_unregistered;
}

void Command::AllowUnregistered(bool b) {
    this->allow_unregistered = b;
}

bool Command::RequireUser() const {
    return this->require_user;
}

void Command::RequireUser(bool b) {
    this->require_user = b;
}

const Anope::string Command::GetDesc(CommandSource &) const {
    return this->desc;
}

void Command::OnServHelp(CommandSource &source) {
    source.Reply("    %-14s %s", source.command.c_str(),
                 Language::Translate(source.nc, this->GetDesc(source).c_str()));
}

bool Command::OnHelp(CommandSource &source, const Anope::string &subcommand) {
    return false;
}

void Command::OnSyntaxError(CommandSource &source,
                            const Anope::string &subcommand) {
    this->SendSyntax(source);
    bool has_help = source.service->commands.find("HELP") !=
                    source.service->commands.end();
    if (has_help) {
        source.Reply(MORE_INFO, Config->StrictPrivmsg.c_str(),
                     source.service->nick.c_str(), source.command.c_str());
    }
}

void Command::Run(CommandSource &source, const Anope::string &message) {
    std::vector<Anope::string> params;
    spacesepstream(message).GetTokens(params);
    bool has_help = source.service->commands.find("HELP") !=
                    source.service->commands.end();

    CommandInfo::map::const_iterator it = source.service->commands.end();
    unsigned count = 0;
    for (unsigned max = params.size(); it == source.service->commands.end()
            && max > 0; --max) {
        Anope::string full_command;
        for (unsigned i = 0; i < max; ++i) {
            full_command += " " + params[i];
        }
        full_command.erase(full_command.begin());

        ++count;
        it = source.service->commands.find(full_command);
    }

    if (it == source.service->commands.end()) {
        if (has_help) {
            source.Reply(_("Unknown command \002%s\002. \"%s%s HELP\" for help."),
                         message.c_str(), Config->StrictPrivmsg.c_str(), source.service->nick.c_str());
        } else {
            source.Reply(_("Unknown command \002%s\002."), message.c_str());
        }
        return;
    }

    const CommandInfo &info = it->second;
    ServiceReference<Command> c("Command", info.name);
    if (!c) {
        if (has_help) {
            source.Reply(_("Unknown command \002%s\002. \"%s%s HELP\" for help."),
                         message.c_str(), Config->StrictPrivmsg.c_str(), source.service->nick.c_str());
        } else {
            source.Reply(_("Unknown command \002%s\002."), message.c_str());
        }
        Log(source.service) << "Command " << it->first <<
                            " exists on me, but its service " << info.name << " was not found!";
        return;
    }

    for (unsigned i = 0, j = params.size() - (count - 1); i < j; ++i) {
        params.erase(params.begin());
    }

    while (c->max_params > 0 && params.size() > c->max_params) {
        params[c->max_params - 1] += " " + params[c->max_params];
        params.erase(params.begin() + c->max_params);
    }

    c->Run(source, it->first, info, params);
}

void Command::Run(CommandSource &source, const Anope::string &cmdname,
                  const CommandInfo &info, std::vector<Anope::string> &params) {
    if (this->RequireUser() && !source.GetUser()) {
        return;
    }

    // Command requires registered users only
    if (!this->AllowUnregistered() && !source.nc) {
        source.Reply(NICK_IDENTIFY_REQUIRED);
        if (source.GetUser()) {
            Log(LOG_NORMAL, "access_denied_unreg",
                source.service) << "Access denied for unregistered user " <<
                                source.GetUser()->GetMask() << " with command " << cmdname;
        }
        return;
    }

    source.command = cmdname;
    source.permission = info.permission;

    EventReturn MOD_RESULT;
    FOREACH_RESULT(OnPreCommand, MOD_RESULT, (source, this, params));
    if (MOD_RESULT == EVENT_STOP) {
        return;
    }

    if (params.size() < this->min_params) {
        this->OnSyntaxError(source, !params.empty() ? params[params.size() - 1] : "");
        return;
    }

    // If the command requires a permission, and they aren't registered or don't have the required perm, DENIED
    if (!info.permission.empty() && !source.HasCommand(info.permission)) {
        source.Reply(ACCESS_DENIED);
        if (source.GetUser()) {
            Log(LOG_NORMAL, "access_denied",
                source.service) << "Access denied for user " << source.GetUser()->GetMask() <<
                                " with command " << cmdname;
        }
        return;
    }

    this->Execute(source, params);
    FOREACH_MOD(OnPostCommand, (source, this, params));
}

bool Command::FindCommandFromService(const Anope::string &command_service,
                                     BotInfo* &bot, Anope::string &name) {
    bot = NULL;

    for (botinfo_map::iterator it = BotListByNick->begin(),
            it_end = BotListByNick->end(); it != it_end; ++it) {
        BotInfo *bi = it->second;

        for (CommandInfo::map::const_iterator cit = bi->commands.begin(),
                cit_end = bi->commands.end(); cit != cit_end; ++cit) {
            const Anope::string &c_name = cit->first;
            const CommandInfo &info = cit->second;

            if (info.name != command_service) {
                continue;
            }

            bot = bi;
            name = c_name;
            return true;
        }
    }

    return false;
}
