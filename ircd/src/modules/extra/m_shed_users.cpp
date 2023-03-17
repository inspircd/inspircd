/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemonirc@gmail.com>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/// $ModAuthor: linuxdaemon
/// $ModAuthorMail: linuxdaemonirc@gmail.com
/// $ModDepends: core 3
/// $ModDesc: Slowly disconnects idle users for maintenance
/// $ModConfig: <shedusers shedopers="no" kill="yes" shutdown="no" blockconnect="yes" minidle="3600" maxusers="0" message="This server has entered maintenance mode." blockmessage="This server is in maintenance mode.">

// Maintenance mode can be triggered with the /SHEDUSERS command
// as well as sending SIGUSR2 to the inspircd process

// If shedding is enabled, the inspircd.org/shedding cap will be advertised

// If shedding is enabled while a user is online, the user will received a CAP ADD with the cap,
// if capnotify is available

// Shedding can also be controlled via the HTTPApi
// - /shedding or /shedding/status - Get the current shedding status
// - /shedding/start - Enable shedding
// - /shedding/stop - Disable shedding
//
// Note: Requires m_httpd be loaded, the rest of the module will work without it though

#include "inspircd.h"
#include "exitcodes.h"
#include "modules/cap.h"
#include "modules/httpd.h"

#define CAP_NAME "inspircd.org/shedding"

inline unsigned long GetIdle(LocalUser* lu) {
    return ServerInstance->Time() - lu->idle_lastmsg;
}

static volatile sig_atomic_t active;
static volatile sig_atomic_t notified;

static Module* me;

bool IsShedding() {
    return active;
}

bool HasNotified() {
    return notified;
}

void SetNotified(bool b) {
    notified = b;
}

Cap::Capability* GetCap() {
    if (!me) {
        return NULL;
    }

    dynamic_reference<Cap::Capability> ref(me, "cap/" CAP_NAME);
    if (!ref) {
        return NULL;
    }

    return *ref;
}

void StartShedding() {
    if (IsShedding()) {
        return;
    }

    active = 1;
    SetNotified(false);
    Cap::Capability* cap = GetCap();
    if (cap) {
        cap->SetActive(true);
    }
}

void StopShedding() {
    active = 0;
    SetNotified(false);
    Cap::Capability* cap = GetCap();
    if (cap) {
        cap->SetActive(false);
    }
}

class CommandShed
    : public Command {
    bool enable;
  public:
    CommandShed(Module* Mod, const std::string& Name, bool Enable)
        : Command(Mod, Name, 0, 1)
        , enable(Enable) {
        flags_needed = 'o';
        syntax = "[servermask]";
    }

    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        if (parameters.empty() || InspIRCd::Match(ServerInstance->Config->ServerName, parameters[0])) {
            if (enable) {
                StartShedding();
            } else {
                StopShedding();
            }
        }

        return CMD_SUCCESS;
    }

    RouteDescriptor GetRouting(User* user,
                               const CommandBase::Params& parameters) CXX11_OVERRIDE {
        if (parameters.empty()) {
            return ROUTE_LOCALONLY;
        }
        return ROUTE_OPT_BCAST;
    }
};

class SheddingHTTPApi
    : public HTTPRequestEventListener {
    Module* creator;
    HTTPdAPI httpd;
    const std::string urlprefix;

    enum Endpoint {
        STATUS,
        START,
        STOP,

        NOT_FOUND,
        WRONG_PREFIX
    };

  public:
    SheddingHTTPApi(Module* Creator, const std::string& Urlprefix = "/shedding")
        : HTTPRequestEventListener(Creator)
        , creator(Creator)
        , httpd(Creator)
        , urlprefix(Urlprefix) {
    }

    Endpoint PathToEndpoint(const std::string& path) const {
        if (!(path == urlprefix
                || path.compare(0, urlprefix.length() + 1, urlprefix + "/") == 0)) {
            return WRONG_PREFIX;
        }

        // `path` is either <urlprefix> or <urlprefix>/*

        // Remove `urlprefix`
        std::string stripped = path.substr(urlprefix.length());

        if (!stripped.empty()) {
            stripped.erase(0, 1);
            if (!stripped.empty() && *stripped.rbegin() == '/') {
                stripped.erase(stripped.size() - 1, 1);
            }
        }


        if (stripped.empty() || stripped == "status") {
            return STATUS;
        } else if (stripped == "start") {
            return START;
        } else if (stripped == "stop") {
            return STOP;
        }

        return NOT_FOUND;
    }

    ModResult OnHTTPRequest(HTTPRequest& req) CXX11_OVERRIDE {
        Endpoint endpoint = PathToEndpoint(req.GetPath());

        std::stringstream sstr;

        switch (endpoint) {
        case STATUS:
            sstr << "{\"active\":" << (IsShedding() ? "true" : "false") << "}";
            break;
        case START:
            if (IsShedding()) {
                sstr << "{\"error\":\"already_shedding\",\"message\":\"Shedding is already enabled\"}";
            } else {
                StartShedding();
                sstr << "{\"status\":\"success\"}";
            }
            break;
        case STOP:
            if (!IsShedding()) {
                sstr << "{\"error\":\"not_shedding\",\"message\":\"Shedding is not enabled\"}";
            } else {
                StopShedding();
                sstr << "{\"status\":\"success\"}";
            }
            break;
        case NOT_FOUND:
            sstr << "{\"error\":\"unknown_action\"}";
            break;
        case WRONG_PREFIX:
            return MOD_RES_PASSTHRU;
            break;
        }

        /* Send the document back to m_httpd */
        HTTPDocumentResponse response(creator, req, &sstr, endpoint != NOT_FOUND ? 200 : 404);
        response.headers.SetHeader("X-Powered-By", MODNAME);
        response.headers.SetHeader("Content-Type", "application/json");
        httpd->SendResponse(response);
        return MOD_RES_DENY; // Handled
    }
};

class ModuleShedUsers
    : public Module {
  public:
    static void sighandler(int) {
        StartShedding();
    }

  private:
    CommandShed startcmd, stopcmd;
    Cap::Capability cap;
    SheddingHTTPApi httpapi;

    std::string message;
    std::string blockmessage;

    unsigned long maxusers;
    unsigned long minidle;

    bool shedopers;
    bool shutdown;
    bool blockconnects;
    bool kill;

  public:
    ModuleShedUsers()
        : startcmd(this, "SHEDUSERS", true)
        , stopcmd(this, "STOPSHED", false)
        , cap(this, CAP_NAME)
        , httpapi(this, "/shedding")
        , maxusers(0)
        , minidle(0)
        , shedopers(false)
        , shutdown(false)
        , blockconnects(false)
        , kill(false) {
        me = this;
    }

    void init() CXX11_OVERRIDE {
        StopShedding();
        signal(SIGUSR2, sighandler);
    }

    ~ModuleShedUsers() CXX11_OVERRIDE {
        signal(SIGUSR2, SIG_IGN);
        me = NULL;
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("shedusers");

        message = tag->getString("message", "This server has entered maintenance mode.");
        blockmessage = tag->getString("blockmessage", "This server is in maintenance mode.");
        maxusers = tag->getUInt("maxusers", 0);
        minidle = tag->getDuration("minidle", 60, 1);
        shedopers = tag->getBool("shedopers");
        shutdown = tag->getBool("shutdown");
        blockconnects = tag->getBool("blockconnect", true);
        kill = tag->getBool("kill", true);
    }

    bool CanShed(LocalUser* lu) const {
        if (!shedopers && lu->IsOper()) {
            return false;
        }

        if (lu->registered != REG_ALL) {
            return false;
        }

        if (GetIdle(lu) < minidle) {
            return false;
        }

        return true;
    }

    void OnSetUserIP(LocalUser* user) CXX11_OVERRIDE {
        if (IsShedding() && blockconnects && user->registered != REG_ALL) {
            ServerInstance->Users.QuitUser(user, blockmessage);
        }
    }

    void OnBackgroundTimer(time_t) CXX11_OVERRIDE {
        if (!IsShedding()) {
            return;
        }

        if (!HasNotified()) {
            ClientProtocol::Messages::Privmsg msg(ClientProtocol::Messages::Privmsg::nocopy,
                                                  ServerInstance->FakeClient, ServerInstance->Config->ServerName, message,
                                                  MSG_NOTICE);
            ClientProtocol::Event msgevent(ServerInstance->GetRFCEvents().privmsg, msg);

            for (UserManager::LocalList::const_iterator i =
                    ServerInstance->Users.GetLocalUsers().begin();
                    i != ServerInstance->Users.GetLocalUsers().end(); ++i) {
                LocalUser* user = *i;
                user->Send(msgevent);
            }
            SetNotified(true);
        }

        if (ServerInstance->Users.LocalUserCount() <= maxusers) {
            if (shutdown) {
                ServerInstance->Exit(EXIT_STATUS_NOERROR);
            }

            StopShedding();
            return;
        }

        if (!kill) {
            return;
        }

        LocalUser* to_quit = NULL;
        const UserManager::LocalList& localusers = ServerInstance->Users.GetLocalUsers();
        for (UserManager::LocalList::const_iterator it = localusers.begin(); it != localusers.end(); ++it) {
            LocalUser* lu = *it;
            if (CanShed(lu) && (!to_quit || lu->idle_lastmsg < to_quit->idle_lastmsg)) {
                to_quit = lu;
            }
        }

        if (!to_quit) {
            return;
        }

        ServerInstance->Users.QuitUser(to_quit, message);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Slowly disconnects idle users for maintenance");
    }
};

MODULE_INIT(ModuleShedUsers)
