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

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "services.h"
#include "anope.h"
#include "service.h"
#include "modes.h"

/* Encapsulates the IRCd protocol we are speaking. */
class CoreExport IRCDProto : public Service {
    Anope::string proto_name;

  protected:
    IRCDProto(Module *creator, const Anope::string &proto_name);
  public:
    virtual ~IRCDProto();

    virtual void SendSVSKillInternal(const MessageSource &, User *,
                                     const Anope::string &);
    virtual void SendModeInternal(const MessageSource &, const Channel *,
                                  const Anope::string &);
    virtual void SendModeInternal(const MessageSource &, User *,
                                  const Anope::string &);
    virtual void SendKickInternal(const MessageSource &, const Channel *, User *,
                                  const Anope::string &);
    virtual void SendNoticeInternal(const MessageSource &,
                                    const Anope::string &dest, const Anope::string &msg);
    virtual void SendPrivmsgInternal(const MessageSource &,
                                     const Anope::string &dest, const Anope::string &buf);
    virtual void SendQuitInternal(User *, const Anope::string &buf);
    virtual void SendPartInternal(User *, const Channel *chan,
                                  const Anope::string &buf);
    virtual void SendGlobopsInternal(const MessageSource &,
                                     const Anope::string &buf);
    virtual void SendCTCPInternal(const MessageSource &, const Anope::string &dest,
                                  const Anope::string &buf);
    virtual void SendNumericInternal(int numeric, const Anope::string &dest,
                                     const Anope::string &buf);

    const Anope::string &GetProtocolName();
    virtual bool Parse(const Anope::string &, Anope::map<Anope::string> &,
                       Anope::string &, Anope::string &, std::vector<Anope::string> &);
    virtual Anope::string Format(const Anope::string &source,
                                 const Anope::string &message);

    /* Modes used by default by our clients */
    Anope::string DefaultPseudoclientModes;
    /* Can we force change a users's nick? */
    bool CanSVSNick;
    /* Can we force join or part users? */
    bool CanSVSJoin;
    /* Can we set vhosts/vidents on users? */
    bool CanSetVHost, CanSetVIdent;
    /* Can we ban specific gecos from being used? */
    bool CanSNLine;
    /* Can we ban specific nicknames from being used? */
    bool CanSQLine;
    /* Can we ban specific channel names from being used? */
    bool CanSQLineChannel;
    /* Can we ban by IP? */
    bool CanSZLine;
    /* Can we place temporary holds on specific nicknames? */
    bool CanSVSHold;
    /* See os_oline */
    bool CanSVSO;
    /* See ns_cert */
    bool CanCertFP;
    /* Whether this IRCd requires unique IDs for each user or server. See TS6/P10. */
    bool RequiresID;
    /* If this IRCd has unique ids, whether the IDs and nicknames are ambiguous */
    bool AmbiguousID;
    /* The maximum number of modes we are allowed to set with one MODE command */
    unsigned MaxModes;
    /* The maximum number of bytes a line may have */
    unsigned MaxLine;

    /* Retrieves the next free UID or SID */
    virtual Anope::string UID_Retrieve();
    virtual Anope::string SID_Retrieve();

    /** Sets the server in NOOP mode. If NOOP mode is enabled, no users
     * will be able to oper on the server.
     * @param s The server
     * @param mode Whether to turn NOOP on or off
     */
    virtual void SendSVSNOOP(const Server *s, bool mode) { }

    /** Sets the topic on a channel
     * @param bi The bot to set the topic from
     * @param c The channel to set the topic on. The topic being set is Channel::topic
     */
    virtual void SendTopic(const MessageSource &, Channel *);

    /** Sets a vhost on a user.
     * @param u The user
     * @param vident The ident to set
     * @param vhost The vhost to set
     */
    virtual void SendVhost(User *u, const Anope::string &vident,
                           const Anope::string &vhost) { }
    virtual void SendVhostDel(User *) { }

    /** Sets an akill. This is a recursive function that can be called multiple times
     * for the same xline, but for different users, if the xline is not one that can be
     * enforced by the IRCd, such as a nick/user/host/realname combination ban.
     * @param u The user affected by the akill, if known
     * @param x The akill
     */
    virtual void SendAkill(User *, XLine *) = 0;
    virtual void SendAkillDel(const XLine *) = 0;

    /* Realname ban */
    virtual void SendSGLine(User *, const XLine *) { }
    virtual void SendSGLineDel(const XLine *) { }

    /* IP ban */
    virtual void SendSZLine(User *u, const XLine *) { }
    virtual void SendSZLineDel(const XLine *) { }

    /* Nick ban (and sometimes channel) */
    virtual void SendSQLine(User *, const XLine *x) { }
    virtual void SendSQLineDel(const XLine *x) { }

    virtual void SendKill(const MessageSource &source, const Anope::string &target,
                          const Anope::string &reason);

    /** Kills a user
     * @param source Who is doing the kill
     * @param user The user to be killed
     * @param fmt Kill reason
     */
    virtual void SendSVSKill(const MessageSource &source, User *user,
                             const char *fmt, ...);

    virtual void SendMode(const MessageSource &source, const Channel *dest,
                          const char *fmt, ...);
    virtual void SendMode(const MessageSource &source, User *u, const char *fmt,
                          ...);

    /** Introduces a client to the rest of the network
     * @param u The client to introduce
     */
    virtual void SendClientIntroduction(User *u) = 0;

    virtual void SendKick(const MessageSource &source, const Channel *chan,
                          User *user, const char *fmt, ...);

    virtual void SendNotice(const MessageSource &source, const Anope::string &dest,
                            const char *fmt, ...);
    virtual void SendPrivmsg(const MessageSource &source, const Anope::string &dest,
                             const char *fmt, ...);
    virtual void SendAction(const MessageSource &source, const Anope::string &dest,
                            const char *fmt, ...);
    virtual void SendCTCP(const MessageSource &source, const Anope::string &dest,
                          const char *fmt, ...);

    virtual void SendGlobalNotice(BotInfo *bi, const Server *dest,
                                  const Anope::string &msg) = 0;
    virtual void SendGlobalPrivmsg(BotInfo *bi, const Server *desc,
                                   const Anope::string &msg) = 0;

    virtual void SendQuit(User *u, const char *fmt, ...);
    virtual void SendPing(const Anope::string &servname, const Anope::string &who);
    virtual void SendPong(const Anope::string &servname, const Anope::string &who);

    /** Joins one of our users to a channel.
     * @param u The user to join
     * @param c The channel to join the user to
     * @param status The status to set on the user after joining. This may or may not already internally
     * be set on the user. This may include the modes in the join, but will usually place them on the mode
     * stacker to be set "soon".
     */
    virtual void SendJoin(User *u, Channel *c, const ChannelStatus *status) = 0;
    virtual void SendPart(User *u, const Channel *chan, const char *fmt, ...);

    /** Force joins a user that isn't ours to a channel.
     * @param bi The source of the message
     * @param u The user to join
     * @param chan The channel to join the user to
     * @param param Channel key?
     */
    virtual void SendSVSJoin(const MessageSource &source, User *u,
                             const Anope::string &chan, const Anope::string &param) { }

    /** Force parts a user that isn't ours from a channel.
     * @param source The source of the message
     * @param u The user to part
     * @param chan The channel to part the user from
     * @param param part reason, some IRCds don't support this
     */
    virtual void SendSVSPart(const MessageSource &source, User *u,
                             const Anope::string &chan, const Anope::string &param) { }

    virtual void SendInvite(const MessageSource &source, const Channel *c, User *u);
    virtual void SendGlobops(const MessageSource &source, const char *fmt, ...);

    /** Sets oper flags on a user, currently only supported by Unreal
     */
    virtual void SendSVSO(BotInfo *, const Anope::string &, const Anope::string &) { }

    /** Sends a nick change of one of our clients.
     */
    virtual void SendNickChange(User *u, const Anope::string &newnick);

    /** Forces a nick change of a user that isn't ours (SVSNICK)
     */
    virtual void SendForceNickChange(User *u, const Anope::string &newnick,
                                     time_t when);

    /** Used to introduce ourselves to our uplink. Usually will SendServer(Me) and any other
     * initial handshake requirements.
     */
    virtual void SendConnect() = 0;

    /** Called right before we begin our burst, after we have handshaked successfully with the uplink.
     * At this point none of our servers, users, or channels exist on the uplink
     */
    virtual void SendBOB() { }
    virtual void SendEOB() { }

    virtual void SendSVSHold(const Anope::string &, time_t) { }
    virtual void SendSVSHoldDel(const Anope::string &) { }

    virtual void SendSWhois(const MessageSource &, const Anope::string &,
                            const Anope::string &) { }

    /** Introduces a server to the uplink
     */
    virtual void SendServer(const Server *) = 0;
    virtual void SendSquit(Server *, const Anope::string &message);

    virtual void SendNumeric(int numeric, const Anope::string &dest,
                             const char *fmt, ...);

    virtual void SendLogin(User *u, NickAlias *na) = 0;
    virtual void SendLogout(User *u) = 0;

    /** Send a channel creation message to the uplink.
     * On most TS6 IRCds this is a SJOIN with no nick
     */
    virtual void SendChannel(Channel *c) { }

    /** Make the user an IRC operator
     * Normally this is a simple +o, though some IRCds require us to send the oper type
     */
    virtual void SendOper(User *u);

    virtual void SendSASLMechanisms(std::vector<Anope::string> &) { }
    virtual void SendSASLMessage(const SASL::Message &) { }
    virtual void SendSVSLogin(const Anope::string &uid, const Anope::string &acc,
                              const Anope::string &vident, const Anope::string &vhost) { }

    virtual bool IsNickValid(const Anope::string &);
    virtual bool IsChannelValid(const Anope::string &);
    virtual bool IsIdentValid(const Anope::string &);
    virtual bool IsHostValid(const Anope::string &);
    virtual bool IsExtbanValid(const Anope::string &) {
        return false;
    }

    /** Retrieve the maximum number of list modes settable on this channel
     * Defaults to Config->ListSize
     */
    virtual unsigned GetMaxListFor(Channel *c);
    virtual unsigned GetMaxListFor(Channel *c, ChannelMode *cm);

    virtual Anope::string NormalizeMask(const Anope::string &mask);
};

class CoreExport MessageSource {
    Anope::string source;
    User *u;
    Server *s;

  public:
    MessageSource(const Anope::string &);
    MessageSource(User *u);
    MessageSource(Server *s);
    const Anope::string &GetName() const;
    const Anope::string &GetSource() const;
    User *GetUser() const;
    BotInfo *GetBot() const;
    Server *GetServer() const;
};

enum IRCDMessageFlag {
    IRCDMESSAGE_SOFT_LIMIT,
    IRCDMESSAGE_REQUIRE_SERVER,
    IRCDMESSAGE_REQUIRE_USER
};

class CoreExport IRCDMessage : public Service {
    Anope::string name;
    unsigned param_count;
    std::set<IRCDMessageFlag> flags;
  public:
    IRCDMessage(Module *owner, const Anope::string &n, unsigned p = 0);
    unsigned GetParamCount() const;
    virtual void Run(MessageSource &, const std::vector<Anope::string> &params) = 0;
    virtual void Run(MessageSource &, const std::vector<Anope::string> &params,
                     const Anope::map<Anope::string> &tags);

    void SetFlag(IRCDMessageFlag f) {
        flags.insert(f);
    }
    bool HasFlag(IRCDMessageFlag f) const {
        return flags.count(f);
    }
};

/** MessageTokenizer allows tokens in the IRC wire format to be read from a string */
class CoreExport MessageTokenizer {
  private:
    /** The message we are parsing tokens from. */
    Anope::string message;

    /** The current position within the message. */
    Anope::string::size_type position;

  public:
    /** Create a tokenstream and fill it with the provided data. */
    MessageTokenizer(const Anope::string &msg);

    /** Retrieve the next \<middle> token in the message.
     * @param token The next token available, or an empty string if none remain.
     * @return True if a token was retrieved; otherwise, false.
     */
    bool GetMiddle(Anope::string &token);

    /** Retrieve the next \<trailing> token in the message.
     * @param token The next token available, or an empty string if none remain.
     * @return True if a token was retrieved; otherwise, false.
     */
    bool GetTrailing(Anope::string &token);
};

extern CoreExport IRCDProto *IRCD;

#endif // PROTOCOL_H
