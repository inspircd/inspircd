/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018-2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012-2013 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 John Brooks <special@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005, 2007 Craig Edwards <brain@inspircd.org>
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


#include "inspircd.h"

enum {
    // Either the ident lookup has not started yet or the user is registered.
    IDENT_UNKNOWN = 0,

    // Ident lookups are not enabled and a user has been marked as being skipped.
    IDENT_SKIPPED,

    // Ident lookups are not enabled and a user has been an insecure ident prefix.
    IDENT_PREFIXED,

    // An ident lookup was done and an ident was found.
    IDENT_FOUND,

    // An ident lookup was done but no ident was found
    IDENT_MISSING
};

/* --------------------------------------------------------------
 * Note that this is the third incarnation of m_ident. The first
 * two attempts were pretty crashy, mainly due to the fact we tried
 * to use InspSocket/BufferedSocket to make them work. This class
 * is ok for more heavyweight tasks, it does a lot of things behind
 * the scenes that are not good for ident sockets and it has a huge
 * memory footprint!
 *
 * To fix all the issues that we had in the old ident modules (many
 * nasty race conditions that would cause segfaults etc) we have
 * rewritten this module to use a simplified socket object based
 * directly off EventHandler. As EventHandler only has low level
 * readability, writability and error events tied directly to the
 * socket engine, this makes our lives easier as nothing happens to
 * our ident lookup class that is outside of this module, or out-
 * side of the control of the class. There are no timers, internal
 * events, or such, which will cause the socket to be deleted,
 * queued for deletion, etc. In fact, there's not even any queueing!
 *
 * Using this framework we have a much more stable module.
 *
 * A few things to note:
 *
 *   O  The only place that may *delete* an active or inactive
 *      ident socket is OnUserDisconnect in the module class.
 *      Because this is out of scope of the socket class there is
 *      no possibility that the socket may ever try to delete
 *      itself.
 *
 *   O  Closure of the ident socket with the Close() method will
 *      not cause removal of the socket from memory or detachment
 *      from its 'parent' User class. It will only flag it as an
 *      inactive socket in the socket engine.
 *
 *   O  Timeouts are handled in OnCheckReady at the same time as
 *      checking if the ident socket has a result. This is done
 *      by checking if the age the of the class (its instantiation
 *      time) plus the timeout value is greater than the current time.
 *
 *  O   The ident socket is able to but should not modify its
 *      'parent' user directly. Instead the ident socket class sets
 *      a completion flag and during the next call to OnCheckReady,
 *      the completion flag will be checked and any result copied to
 *      that user's class. This again ensures a single point of socket
 *      deletion for safer, neater code.
 *
 *  O   The code in the constructor of the ident socket is taken from
 *      BufferedSocket but majorly thinned down. It works for both
 *      IPv4 and IPv6.
 *
 *  O   In the event that the ident socket throws a ModuleException,
 *      nothing is done. This is counted as total and complete
 *      failure to create a connection.
 * --------------------------------------------------------------
 */

class IdentRequestSocket : public EventHandler {
  public:
    LocalUser *user;            /* User we are attached to */
    std::string result;     /* Holds the ident string if done */
    time_t age;
    bool done;          /* True if lookup is finished */

    IdentRequestSocket(LocalUser* u) : user(u) {
        age = ServerInstance->Time();

        SetFd(socket(user->server_sa.family(), SOCK_STREAM, 0));
        if (!HasFd()) {
            throw ModuleException("Could not create socket");
        }

        done = false;

        irc::sockets::sockaddrs bindaddr;
        irc::sockets::sockaddrs connaddr;

        memcpy(&bindaddr, &user->server_sa, sizeof(bindaddr));
        memcpy(&connaddr, &user->client_sa, sizeof(connaddr));

        if (connaddr.family() == AF_INET6) {
            bindaddr.in6.sin6_port = 0;
            connaddr.in6.sin6_port = htons(113);
        } else {
            bindaddr.in4.sin_port = 0;
            connaddr.in4.sin_port = htons(113);
        }

        /* Attempt to bind (ident requests must come from the ip the query is referring to */
        if (SocketEngine::Bind(GetFd(), bindaddr) < 0) {
            this->Close();
            throw ModuleException("failed to bind()");
        }

        SocketEngine::NonBlocking(GetFd());

        /* Attempt connection (nonblocking) */
        if (SocketEngine::Connect(this, connaddr) == -1 && errno != EINPROGRESS) {
            this->Close();
            throw ModuleException("connect() failed");
        }

        /* Add fd to socket engine */
        if (!SocketEngine::AddFd(this, FD_WANT_NO_READ | FD_WANT_POLL_WRITE)) {
            this->Close();
            throw ModuleException("out of fds");
        }
    }

    void OnEventHandlerWrite() CXX11_OVERRIDE {
        SocketEngine::ChangeEventMask(this, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);

        char req[32];

        /* Build request in the form 'localport,remoteport\r\n' */
        int req_size;
        if (user->client_sa.family() == AF_INET6)
            req_size = snprintf(req, sizeof(req), "%d,%d\r\n",
                                ntohs(user->client_sa.in6.sin6_port), ntohs(user->server_sa.in6.sin6_port));
        else
            req_size = snprintf(req, sizeof(req), "%d,%d\r\n",
                                ntohs(user->client_sa.in4.sin_port), ntohs(user->server_sa.in4.sin_port));

        /* Send failed if we didnt write the whole ident request --
         * might as well give up if this happens!
         */
        if (SocketEngine::Send(this, req, req_size, 0) < req_size) {
            done = true;
        }
    }

    void Close() {
        /* Remove ident socket from engine, and close it, but dont detach it
         * from its parent user class, or attempt to delete its memory.
         */
        if (HasFd()) {
            ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Close ident socket %d", GetFd());
            SocketEngine::Close(this);
        }
    }

    bool HasResult() {
        return done;
    }

    void OnEventHandlerRead() CXX11_OVERRIDE {
        /* We don't really need to buffer for incomplete replies here, since IDENT replies are
         * extremely short - there is *no* sane reason it'd be in more than one packet
         */
        char ibuf[256];
        int recvresult = SocketEngine::Recv(this, ibuf, sizeof(ibuf)-1, 0);

        /* Close (but don't delete from memory) our socket
         * and flag as done since the ident lookup has finished
         */
        Close();
        done = true;

        /* Cant possibly be a valid response shorter than 3 chars,
         * because the shortest possible response would look like: '1,1'
         */
        if (recvresult < 3) {
            return;
        }

        ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "ReadResponse()");

        /* Truncate at the first null character, but first make sure
         * there is at least one null char (at the end of the buffer).
         */
        ibuf[recvresult] = '\0';
        std::string buf(ibuf);

        /* <2 colons: invalid
         *  2 colons: reply is an error
         * >3 colons: there is a colon in the ident
         */
        if (std::count(buf.begin(), buf.end(), ':') != 3) {
            return;
        }

        std::string::size_type lastcolon = buf.rfind(':');

        /* Truncate the ident at any characters we don't like, skip leading spaces */
        for (std::string::const_iterator i = buf.begin()+lastcolon+1; i != buf.end(); ++i) {
            if (result.size() == ServerInstance->Config->Limits.IdentMax)
                /* Ident is getting too long */
            {
                break;
            }

            if (*i == ' ') {
                continue;
            }

            /* Add the next char to the result and see if it's still a valid ident,
             * according to IsIdent(). If it isn't, then erase what we just added and
             * we're done.
             */
            result += *i;
            if (!ServerInstance->IsIdent(result)) {
                result.erase(result.end()-1);
                break;
            }
        }
    }

    void OnEventHandlerError(int errornum) CXX11_OVERRIDE {
        Close();
        done = true;
    }

    CullResult cull() CXX11_OVERRIDE {
        Close();
        return EventHandler::cull();
    }
};

class ModuleIdent : public Module {
  private:
    unsigned int timeout;
    bool prefixunqueried;
    SimpleExtItem<IdentRequestSocket, stdalgo::culldeleter> socket;
    LocalIntExt state;

    static void PrefixIdent(LocalUser* user) {
        // Check that they haven't been prefixed already.
        if (user->ident[0] == '~') {
            return;
        }

        // All invalid usernames are prefixed with a tilde.
        std::string newident(user->ident);
        newident.insert(newident.begin(), '~');

        // If the username is too long then truncate it.
        if (newident.length() > ServerInstance->Config->Limits.IdentMax) {
            newident.erase(ServerInstance->Config->Limits.IdentMax);
        }

        // Apply the new username.
        user->ChangeIdent(newident);
    }

  public:
    ModuleIdent()
        : socket("ident_socket", ExtensionItem::EXT_USER, this)
        , state("ident_state", ExtensionItem::EXT_USER, this) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows the usernames (idents) of users to be looked up using the RFC 1413 Identification Protocol.", VF_VENDOR);
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("ident");
        timeout = tag->getDuration("timeout", 5, 1, 60);
        prefixunqueried = tag->getBool("prefixunqueried");
    }

    void OnSetUserIP(LocalUser* user) CXX11_OVERRIDE {
        IdentRequestSocket* isock = socket.get(user);
        if (isock) {
            // If an ident lookup request was in progress then cancel it.
            isock->Close();
            socket.unset(user);
        }

        // The ident protocol requires that clients are connecting over a protocol with ports.
        if (user->client_sa.family() != AF_INET && user->client_sa.family() != AF_INET6) {
            return;
        }

        // We don't want to look this up once the user has connected.
        if (user->registered == REG_ALL || user->quitting) {
            return;
        }

        ConfigTag* tag = user->MyClass->config;
        if (!tag->getBool("useident", true)) {
            state.set(user, IDENT_SKIPPED);
            return;
        }

        user->WriteNotice("*** Looking up your ident...");

        try {
            isock = new IdentRequestSocket(user);
            socket.set(user, isock);
        } catch (ModuleException &e) {
            ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                      "Ident exception: " + e.GetReason());
        }
    }

    /* This triggers pretty regularly, we can use it in preference to
     * creating a Timer object and especially better than creating a
     * Timer per ident lookup!
     */
    ModResult OnCheckReady(LocalUser *user) CXX11_OVERRIDE {
        /* Does user have an ident socket attached at all? */
        IdentRequestSocket* isock = socket.get(user);
        if (!isock) {
            if (prefixunqueried && state.get(user) == IDENT_SKIPPED) {
                PrefixIdent(user);
                state.set(user, IDENT_PREFIXED);
            }
            return MOD_RES_PASSTHRU;
        }

        time_t compare = isock->age + timeout;

        /* Check for timeout of the socket */
        if (ServerInstance->Time() >= compare) {
            /* Ident timeout */
            state.set(user, IDENT_MISSING);
            PrefixIdent(user);
            user->WriteNotice("*** Ident lookup timed out, using " + user->ident +
                              " instead.");
        } else if (!isock->HasResult()) {
            // time still good, no result yet... hold the registration
            return MOD_RES_DENY;
        }

        /* wooo, got a result (it will be good, or bad) */
        else if (isock->result.empty()) {
            state.set(user, IDENT_MISSING);
            PrefixIdent(user);
            user->WriteNotice("*** Could not find your ident, using " + user->ident +
                              " instead.");
        } else {
            state.set(user, IDENT_FOUND);
            user->ChangeIdent(isock->result);
            user->WriteNotice("*** Found your ident, '" + user->ident + "'");
        }

        isock->Close();
        socket.unset(user);
        return MOD_RES_PASSTHRU;
    }

    ModResult OnSetConnectClass(LocalUser* user,
                                ConnectClass* myclass) CXX11_OVERRIDE {
        if (myclass->config->getBool("requireident") && state.get(user) != IDENT_FOUND) {
            ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG,
                                      "The %s connect class is not suitable as it requires an identd response",
                                      myclass->GetName().c_str());
            return MOD_RES_DENY;
        }
        return MOD_RES_PASSTHRU;
    }

    void OnUserConnect(LocalUser* user) CXX11_OVERRIDE {
        // Clear this as it is no longer necessary.
        state.unset(user);
    }
};

MODULE_INIT(ModuleIdent)
