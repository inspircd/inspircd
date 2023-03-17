/*
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

#include "protocol.h"

/* Common IRCD messages.
 * Protocol modules may chose to include some, none, or all of these handlers
 * as they see fit.
 */

namespace Message {

struct CoreExport Away : IRCDMessage {
    Away(Module *creator, const Anope::string &mname = "AWAY") : IRCDMessage(
            creator, mname, 0) {
        SetFlag(IRCDMESSAGE_REQUIRE_USER);
        SetFlag(IRCDMESSAGE_SOFT_LIMIT);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override;
};

struct CoreExport Capab : IRCDMessage {
    Capab(Module *creator, const Anope::string &mname = "CAPAB") : IRCDMessage(
            creator, mname, 1) {
        SetFlag(IRCDMESSAGE_SOFT_LIMIT);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override;
};

struct CoreExport Error : IRCDMessage {
    Error(Module *creator, const Anope::string &mname = "ERROR") : IRCDMessage(
            creator, mname, 1) { }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override;
};

struct CoreExport Invite : IRCDMessage {
    Invite(Module *creator, const Anope::string &mname = "INVITE") : IRCDMessage(
            creator, mname, 2) {
        SetFlag(IRCDMESSAGE_REQUIRE_USER);
        SetFlag(IRCDMESSAGE_SOFT_LIMIT);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override;
};

struct CoreExport Join : IRCDMessage {
    Join(Module *creator, const Anope::string &mname = "JOIN") : IRCDMessage(
            creator, mname, 1) {
        SetFlag(IRCDMESSAGE_REQUIRE_USER);
        SetFlag(IRCDMESSAGE_SOFT_LIMIT);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override;

    typedef std::pair<ChannelStatus, User *> SJoinUser;

    /** Handle a SJOIN.
     * @param source The source of the SJOIN
     * @param chan The channel the users are joining to
     * @param ts The TS for the channel
     * @param modes The modes sent with the SJOIN, if any
     * @param users The users and their status, if any
     */
    static void SJoin(MessageSource &source, const Anope::string &chan, time_t ts,
                      const Anope::string &modes, const std::list<SJoinUser> &users);
};

struct CoreExport Kick : IRCDMessage {
    Kick(Module *creator, const Anope::string &mname = "KICK") : IRCDMessage(
            creator, mname, 2) {
        SetFlag(IRCDMESSAGE_SOFT_LIMIT);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override;
};

struct CoreExport Kill : IRCDMessage {
    Kill(Module *creator, const Anope::string &mname = "KILL") : IRCDMessage(
            creator, mname, 2) { }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override;
};

struct CoreExport Mode : IRCDMessage {
    Mode(Module *creator, const Anope::string &mname = "MODE") : IRCDMessage(
            creator, mname, 2) {
        SetFlag(IRCDMESSAGE_SOFT_LIMIT);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override;
};

struct CoreExport MOTD : IRCDMessage {
    MOTD(Module *creator, const Anope::string &mname = "MOTD") : IRCDMessage(
            creator, mname, 1) { }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override;
};

struct CoreExport Notice : IRCDMessage {
    Notice(Module *creator, const Anope::string &mname = "NOTICE") : IRCDMessage(
            creator, mname, 2) {
        SetFlag(IRCDMESSAGE_REQUIRE_USER);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override;
};

struct CoreExport Part : IRCDMessage {
    Part(Module *creator, const Anope::string &mname = "PART") : IRCDMessage(
            creator, mname, 1) {
        SetFlag(IRCDMESSAGE_REQUIRE_USER);
        SetFlag(IRCDMESSAGE_SOFT_LIMIT);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override;
};

struct CoreExport Ping : IRCDMessage {
    Ping(Module *creator, const Anope::string &mname = "PING") : IRCDMessage(
            creator, mname, 1) {
        SetFlag(IRCDMESSAGE_SOFT_LIMIT);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override;
};

struct CoreExport Privmsg : IRCDMessage {
    Privmsg(Module *creator,
            const Anope::string &mname = "PRIVMSG") : IRCDMessage(creator, mname, 2) {
        SetFlag(IRCDMESSAGE_REQUIRE_USER);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override;
};

struct CoreExport Quit : IRCDMessage {
    Quit(Module *creator, const Anope::string &mname = "QUIT") : IRCDMessage(
            creator, mname, 1) {
        SetFlag(IRCDMESSAGE_REQUIRE_USER);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override;
};

struct CoreExport SQuit : IRCDMessage {
    SQuit(Module *creator, const Anope::string &mname = "SQUIT") : IRCDMessage(
            creator, mname, 2) { }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override;
};

struct CoreExport Stats : IRCDMessage {
    Stats(Module *creator, const Anope::string &mname = "STATS") : IRCDMessage(
            creator, mname, 1) {
        SetFlag(IRCDMESSAGE_REQUIRE_USER);
        SetFlag(IRCDMESSAGE_SOFT_LIMIT);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override;
};

struct CoreExport Time : IRCDMessage {
    Time(Module *creator, const Anope::string &mname = "TIME") : IRCDMessage(
            creator, mname, 0) {
        SetFlag(IRCDMESSAGE_SOFT_LIMIT);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override;
};

struct CoreExport Topic : IRCDMessage {
    Topic(Module *creator, const Anope::string &mname = "TOPIC") : IRCDMessage(
            creator, mname, 2) {
        SetFlag(IRCDMESSAGE_REQUIRE_USER);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override;
};

struct CoreExport Version : IRCDMessage {
    Version(Module *creator,
            const Anope::string &mname = "VERSION") : IRCDMessage(creator, mname, 0) {
        SetFlag(IRCDMESSAGE_SOFT_LIMIT);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override;
};

struct CoreExport Whois : IRCDMessage {
    Whois(Module *creator, const Anope::string &mname = "WHOIS") : IRCDMessage(
            creator, mname, 1) {
        SetFlag(IRCDMESSAGE_SOFT_LIMIT);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override;
};

} // namespace Message
