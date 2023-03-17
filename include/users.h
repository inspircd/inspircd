/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2012-2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012-2013, 2016-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 DjSlash <djslash@djslash.org>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2011 jackmcbarn <jackmcbarn@inspircd.org>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2008 John Brooks <special@inspircd.org>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2003-2008 Craig Edwards <brain@inspircd.org>
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


#pragma once

#include "socket.h"
#include "inspsocket.h"
#include "mode.h"
#include "membership.h"

/** connect class types
 */
enum ClassTypes {
    /** connect:allow */
    CC_ALLOW = 0,
    /** connect:deny */
    CC_DENY  = 1,
    /** named connect block (for opers, etc) */
    CC_NAMED = 2
};

/** Registration state of a user, e.g.
 * have they sent USER, NICK, PASS yet?
 */
enum RegistrationState {

#ifndef _WIN32   // Burlex: This is already defined in win32, luckily it is still 0.
    REG_NONE = 0,       /* Has sent nothing */
#endif

    REG_USER = 1,       /* Has sent USER */
    REG_NICK = 2,       /* Has sent NICK */
    REG_NICKUSER = 3,   /* Bitwise combination of REG_NICK and REG_USER */
    REG_ALL = 7         /* REG_NICKUSER plus next bit along */
};

enum UserType {
    USERTYPE_LOCAL = 1,
    USERTYPE_REMOTE = 2,
    USERTYPE_SERVER = 3
};

/** Represents \<connect> class tags from the server config */
class CoreExport ConnectClass : public refcountbase {
  public:
    /** The synthesized (with all inheritance applied) config tag this class was read from. */
    reference<ConfigTag> config;

    /** The hosts that this user can connect from as a string. */
    std::string host;

    /** The hosts that this user can connect from as a vector. */
    std::vector<std::string> hosts;

    /** The name of this connect class. */
    std::string name;

    /** If non-empty then the password a user must specify in PASS to be assigned to this class. */
    std::string password;

    /** If non-empty then the hash algorithm that the password field is hashed with. */
    std::string passwordhash;

    /** If non-empty then the server ports which a user has to be connecting on. */
    insp::flat_set<int> ports;

    /** The type of class this. */
    char type;

    /** Whether fake lag is used by this class. */
    bool fakelag:1;

    /** Whether to warn server operators about the limit for this class being reached. */
    bool maxconnwarn:1;

    /** Whether the DNS hostnames of users in this class should be resolved. */
    bool resolvehostnames:1;

    /** Whether this class is for a shared host where the username (ident) uniquely identifies users. */
    bool uniqueusername:1;

    /** The maximum number of channels that users in this class can join. */
    unsigned int maxchans;

    /** The amount of penalty that a user in this class can have before the penalty system activates. */
    unsigned int penaltythreshold;

    /** The number of seconds between keepalive checks for idle clients in this class. */
    unsigned int pingtime;

    /** The number of seconds that connecting users have to register within in this class. */
    unsigned int registration_timeout;

    /** Maximum rate of commands (units: millicommands per second) */
    unsigned int commandrate;

    /** The maximum number of bytes that users in this class can have in their send queue before they are disconnected. */
    unsigned long hardsendqmax;

    /** The maximum number of users in this class that can connect to the local server from one host. */
    unsigned long limit;

    /** The maximum number of users in this class that can connect to the entire network from one host. */
    unsigned long maxglobal;

    /** The maximum number of users that can be in this class on the local server. */
    unsigned long maxlocal;

    /** The maximum number of bytes that users in this class can have in their receive queue before they are disconnected. */
    unsigned long recvqmax;

    /** The maximum number of bytes that users in this class can have in their send queue before their commands stop being processed. */
    unsigned long softsendqmax;

    /** Creates a new connect class from a config tag */
    ConnectClass(ConfigTag* tag, char type, const std::string& mask);

    /** Creates a new connect class with a parent from a config tag. */
    ConnectClass(ConfigTag* tag, char type, const std::string& mask,
                 const ConnectClass& parent);

    /** Update the settings in this block to match the given class */
    void Update(const ConnectClass* newSettings);

    /** Retrieves the name of this connect class. */
    const std::string& GetName() const {
        return name;
    }

    /** Retrieves the hosts that this user can connect from as a string. */
    const std::string& GetHost() const {
        return host;
    }

    /** Retrieves the hosts that this user can connect from as a vector. */
    const std::vector<std::string>& GetHosts() const {
        return hosts;
    }

    /** Returns the registration timeout
     */
    time_t GetRegTimeout() {
        return (registration_timeout ? registration_timeout : 90);
    }

    /** Returns the ping frequency
     */
    unsigned int GetPingTime() {
        return (pingtime ? pingtime : 120);
    }

    /** Returns the maximum sendq value (soft limit)
     * Note that this is in addition to internal OS buffers
     */
    unsigned long GetSendqSoftMax() {
        return (softsendqmax ? softsendqmax : 4096);
    }

    /** Returns the maximum sendq value (hard limit)
     */
    unsigned long GetSendqHardMax() {
        return (hardsendqmax ? hardsendqmax : 0x100000);
    }

    /** Returns the maximum recvq value
     */
    unsigned long GetRecvqMax() {
        return (recvqmax ? recvqmax : 4096);
    }

    /** Returns the penalty threshold value
     */
    unsigned int GetPenaltyThreshold() {
        return penaltythreshold ? penaltythreshold : (fakelag ? 10 : 20);
    }

    unsigned int GetCommandRate() {
        return commandrate ? commandrate : 1000;
    }

    /** Return the maximum number of local sessions
     */
    unsigned long GetMaxLocal() {
        return maxlocal;
    }

    /** Returns the maximum number of global sessions
     */
    unsigned long GetMaxGlobal() {
        return maxglobal;
    }
};

/** An id that can be used to mark the last message sent. */
typedef unsigned int already_sent_t;

/** Holds all information about a user
 * This class stores all information about a user connected to the irc server. Everything about a
 * connection is stored here primarily, from the user's socket ID (file descriptor) through to the
 * user's nickname and hostname.
 */
class CoreExport User : public Extensible {
  private:
    /** Cached nick!ident\@dhost value using the displayed hostname
     */
    std::string cached_fullhost;

    /** Cached ident\@ip value using the real IP address
     */
    std::string cached_hostip;

    /** Cached ident\@realhost value using the real hostname
     */
    std::string cached_makehost;

    /** Cached nick!ident\@realhost value using the real hostname
     */
    std::string cached_fullrealhost;

    /** Set by GetIPString() to avoid constantly re-grabbing IP via sockets voodoo.
     */
    std::string cachedip;

    /** If set then the hostname which is displayed to users. */
    std::string displayhost;

    /** The real hostname of this user. */
    std::string realhost;

    /** The real name of this user. */
    std::string realname;

    /** The user's mode list.
     * Much love to the STL for giving us an easy to use bitset, saving us RAM.
     * if (modes[modeid]) is set, then the mode is set.
     * For example, to work out if mode +i is set, we check the field
     * User::modes[invisiblemode->modeid] == true.
     */
    std::bitset<ModeParser::MODEID_MAX> modes;

  public:
    /** To execute a function for each local neighbor of a user, inherit from this class and
     * pass an instance of it to User::ForEachNeighbor().
     */
    class ForEachNeighborHandler {
      public:
        /** Method to execute for each local neighbor of a user.
         * Derived classes must implement this.
         * @param user Current neighbor
         */
        virtual void Execute(LocalUser* user) = 0;
    };

    /** List of Memberships for this user
     */
    typedef insp::intrusive_list<Membership> ChanList;

    /** Time that the object was instantiated (used for TS calculation etc)
    */
    time_t age;

    /** Time the connection was created, set in the constructor. This
     * may be different from the time the user's classbase object was
     * created.
     */
    time_t signon;

    /** Client address that the user is connected from.
     * Do not modify this value directly, use SetClientIP() to change it.
     * Port is not valid for remote users.
     */
    irc::sockets::sockaddrs client_sa;

    /** The users nickname.
     * An invalid nickname indicates an unregistered connection prior to the NICK command.
     * Use InspIRCd::IsNick() to validate nicknames.
     */
    std::string nick;

    /** The user's unique identifier.
     * This is the unique identifier which the user has across the network.
     */
    const std::string uuid;

    /** The users ident reply.
     * Two characters are added to the user-defined limit to compensate for the tilde etc.
     */
    std::string ident;

    /** What snomasks are set on this user.
     * This functions the same as the above modes.
     */
    std::bitset<64> snomasks;

    /** Channels this user is on
     */
    ChanList chans;

    /** The server the user is connected to.
     */
    Server* server;

    /** The user's away message.
     * If this string is empty, the user is not marked as away.
     */
    std::string awaymsg;

    /** Time the user last went away.
     * This is ONLY RELIABLE if user IsAway()!
     */
    time_t awaytime;

    /** The oper type they logged in as, if they are an oper.
     */
    reference<OperInfo> oper;

    /** Used by User to indicate the registration status of the connection
     * It is a bitfield of the REG_NICK, REG_USER and REG_ALL bits to indicate
     * the connection state.
     */
    unsigned int registered:3;

    /** If this is set to true, then all socket operations for the user
     * are dropped into the bit-bucket.
     * This value is set by QuitUser, and is not needed separately from that call.
     * Please note that setting this value alone will NOT cause the user to quit.
     */
    unsigned int quitting:1;

    /** Whether the ident field uniquely identifies this user on their origin host. */
    bool uniqueusername:1;

    /** What type of user is this? */
    const unsigned int usertype:2;

    /** Get client IP string from sockaddr, using static internal buffer
     * @return The IP string
     */
    const std::string& GetIPString();

    /** Retrieves this user's hostname.
     * @param uncloak If true then return the real host; otherwise, the display host.
     */
    const std::string& GetHost(bool uncloak) const;

    /** Retrieves the username which should be included in bans for this user. */
    const std::string& GetBanIdent() const;

    /** Retrieves this user's displayed hostname. */
    const std::string& GetDisplayedHost() const;

    /** Retrieves this user's real hostname. */
    const std::string& GetRealHost() const;

    /** Retrieves this user's real name. */
    const std::string& GetRealName() const;

    /** Get CIDR mask, using default range, for this user
     */
    irc::sockets::cidr_mask GetCIDRMask();

    /** Sets the client IP for this user
     * @return true if the conversion was successful
     */
    virtual void SetClientIP(const irc::sockets::sockaddrs& sa);

    DEPRECATED_METHOD(virtual bool SetClientIP(const std::string& address));

    /** Constructor
     * @throw CoreException if the UID allocated to the user already exists
     */
    User(const std::string& uid, Server* srv, UserType objtype);

    /** Returns the full displayed host of the user
     * This member function returns the hostname of the user as seen by other users
     * on the server, in nick!ident\@host form.
     * @return The full masked host of the user
     */
    virtual const std::string& GetFullHost();

    /** Returns the full real host of the user
     * This member function returns the hostname of the user as seen by other users
     * on the server, in nick!ident\@host form. If any form of hostname cloaking is in operation,
     * e.g. through a module, then this method will ignore it and return the true hostname.
     * @return The full real host of the user
     */
    virtual const std::string& GetFullRealHost();

    /** This clears any cached results that are used for GetFullRealHost() etc.
     * The results of these calls are cached as generating them can be generally expensive.
     */
    void InvalidateCache();

    /** Returns whether this user is currently away or not. If true,
     * further information can be found in User::awaymsg and User::awaytime
     * @return True if the user is away, false otherwise
     */
    bool IsAway() const {
        return (!awaymsg.empty());
    }

    /** Returns whether this user is an oper or not. If true,
     * oper information can be obtained from User::oper
     * @return True if the user is an oper, false otherwise
     */
    bool IsOper() const {
        return oper;
    }

    /** Returns true if a notice mask is set
     * @param sm A notice mask character to check
     * @return True if the notice mask is set
     */
    bool IsNoticeMaskSet(unsigned char sm);

    /** Get the mode letters of modes set on the user as a string.
     * @param includeparams True to get the parameters of the modes as well. Defaults to false.
     * @return Mode letters of modes set on the user and optionally the parameters of those modes, if any.
     * The returned string always begins with a '+' character. If the user has no modes set, "+" is returned.
     */
    std::string GetModeLetters(bool includeparams = false) const;

    /** Returns true if a specific mode is set
     * @param m The user mode
     * @return True if the mode is set
     */
    bool IsModeSet(unsigned char m) const;
    bool IsModeSet(const ModeHandler* mh) const;
    bool IsModeSet(const ModeHandler& mh) const {
        return IsModeSet(&mh);
    }
    bool IsModeSet(UserModeReference& moderef) const;

    /** Set a specific usermode to on or off
     * @param mh The user mode
     * @param value On or off setting of the mode
     */
    void SetMode(ModeHandler* mh, bool value);
    void SetMode(ModeHandler& mh, bool value) {
        SetMode(&mh, value);
    }

    /** Returns true or false for if a user can execute a privileged oper command.
     * This is done by looking up their oper type from User::oper, then referencing
     * this to their oper classes and checking the commands they can execute.
     * @param command A command (should be all CAPS)
     * @return True if this user can execute the command
     */
    virtual bool HasCommandPermission(const std::string& command);

    /** Returns true if a user has a given permission.
     * This is used to check whether or not users may perform certain actions which admins may not wish to give to
     * all operators, yet are not commands. An example might be oper override, mass messaging (/notice $*), etc.
     *
     * @param privstr The priv to check, e.g. "users/override/topic". These are loaded free-form from the config file.
     * @return True if this user has the permission in question.
     */
    virtual bool HasPrivPermission(const std::string& privstr);

    /** Returns true or false if a user can set a privileged user or channel mode.
     * This is done by looking up their oper type from User::oper, then referencing
     * this to their oper classes, and checking the modes they can set.
     * @param mh Mode to check
     * @return True if the user can set or unset this mode.
     */
    virtual bool HasModePermission(const ModeHandler* mh) const;

    /** Determines whether this user can set the specified snomask.
     * @param chr The server notice mask character to look up.
     * @return True if the user can set the specified snomask; otherwise, false.
     */
    virtual bool HasSnomaskPermission(char chr) const;

    /** Creates a usermask with real host.
     * Takes a buffer to use and fills the given buffer with the hostmask in the format user\@host
     * @return the usermask in the format user\@host
     */
    const std::string& MakeHost();

    /** Creates a usermask with real ip.
     * Takes a buffer to use and fills the given buffer with the ipmask in the format user\@ip
     * @return the usermask in the format user\@ip
     */
    const std::string& MakeHostIP();

    /** Oper up the user using the given opertype.
     * This will also give the +o usermode.
     */
    void Oper(OperInfo* info);

    /** Oper down.
     * This will clear the +o usermode and unset the user's oper type
     */
    void UnOper();

    /** Sends a server notice to this user.
     * @param text The contents of the message to send.
     */
    void WriteNotice(const std::string& text);

    /** Send a NOTICE message from the local server to the user.
     * @param text Text to send
     */
    virtual void WriteRemoteNotice(const std::string& text);

    virtual void WriteRemoteNumeric(const Numeric::Numeric& numeric);

    template <typename T1>
    void WriteRemoteNumeric(unsigned int numeric, T1 p1) {
        Numeric::Numeric n(numeric);
        n.push(p1);
        WriteRemoteNumeric(n);
    }

    template <typename T1, typename T2>
    void WriteRemoteNumeric(unsigned int numeric, T1 p1, T2 p2) {
        Numeric::Numeric n(numeric);
        n.push(p1);
        n.push(p2);
        WriteRemoteNumeric(n);
    }

    template <typename T1, typename T2, typename T3>
    void WriteRemoteNumeric(unsigned int numeric, T1 p1, T2 p2, T3 p3) {
        Numeric::Numeric n(numeric);
        n.push(p1);
        n.push(p2);
        n.push(p3);
        WriteRemoteNumeric(n);
    }

    template <typename T1, typename T2, typename T3, typename T4>
    void WriteRemoteNumeric(unsigned int numeric, T1 p1, T2 p2, T3 p3, T4 p4) {
        Numeric::Numeric n(numeric);
        n.push(p1);
        n.push(p2);
        n.push(p3);
        n.push(p4);
        WriteRemoteNumeric(n);
    }

    template <typename T1, typename T2, typename T3, typename T4, typename T5>
    void WriteRemoteNumeric(unsigned int numeric, T1 p1, T2 p2, T3 p3, T4 p4,
                            T5 p5) {
        Numeric::Numeric n(numeric);
        n.push(p1);
        n.push(p2);
        n.push(p3);
        n.push(p4);
        n.push(p5);
        WriteRemoteNumeric(n);
    }

    void WriteNumeric(const Numeric::Numeric& numeric);

    template <typename T1>
    void WriteNumeric(unsigned int numeric, T1 p1) {
        Numeric::Numeric n(numeric);
        n.push(p1);
        WriteNumeric(n);
    }

    template <typename T1, typename T2>
    void WriteNumeric(unsigned int numeric, T1 p1, T2 p2) {
        Numeric::Numeric n(numeric);
        n.push(p1);
        n.push(p2);
        WriteNumeric(n);
    }

    template <typename T1, typename T2, typename T3>
    void WriteNumeric(unsigned int numeric, T1 p1, T2 p2, T3 p3) {
        Numeric::Numeric n(numeric);
        n.push(p1);
        n.push(p2);
        n.push(p3);
        WriteNumeric(n);
    }

    template <typename T1, typename T2, typename T3, typename T4>
    void WriteNumeric(unsigned int numeric, T1 p1, T2 p2, T3 p3, T4 p4) {
        Numeric::Numeric n(numeric);
        n.push(p1);
        n.push(p2);
        n.push(p3);
        n.push(p4);
        WriteNumeric(n);
    }

    template <typename T1, typename T2, typename T3, typename T4, typename T5>
    void WriteNumeric(unsigned int numeric, T1 p1, T2 p2, T3 p3, T4 p4, T5 p5) {
        Numeric::Numeric n(numeric);
        n.push(p1);
        n.push(p2);
        n.push(p3);
        n.push(p4);
        n.push(p5);
        WriteNumeric(n);
    }

    /** Write to all users that can see this user (including this user in the list if include_self is true), appending CR/LF
     * @param protoev Protocol event to send, may contain any number of messages.
     * @param include_self Should the message be sent back to the author?
     */
    void WriteCommonRaw(ClientProtocol::Event& protoev, bool include_self = true);

    /** Execute a function once for each local neighbor of this user. By default, the neighbors of a user are the users
     * who have at least one common channel with the user. Modules are allowed to alter the set of neighbors freely.
     * This function is used for example to send something conditionally to neighbors, or to send different messages
     * to different users depending on their oper status.
     * @param handler Function object to call, inherited from ForEachNeighborHandler.
     * @param include_self True to include this user in the set of neighbors, false otherwise.
     * Modules may override this. Has no effect if this user is not local.
     */
    already_sent_t ForEachNeighbor(ForEachNeighborHandler& handler,
                                   bool include_self = true);

    /** Return true if the user shares at least one channel with another user
     * @param other The other user to compare the channel list against
     * @return True if the given user shares at least one channel with this user
     */
    bool SharesChannelWith(User *other);

    /** Change the displayed hostname of this user.
     * @param host The new displayed hostname of this user.
     * @return True if the hostname was changed successfully; otherwise, false.
     */
    bool ChangeDisplayedHost(const std::string& host);

    /** Change the real hostname of this user.
     * @param host The new real hostname of this user.
     * @param resetdisplay Whether to reset the display host to this value.
     */
    void ChangeRealHost(const std::string& host, bool resetdisplay);

    /** Change the ident (username) of a user.
     * ALWAYS use this function, rather than writing User::ident directly,
     * as this triggers module events allowing the change to be synchronized to
     * remote servers.
     * @param newident The new ident to set
     * @return True if the change succeeded, false if it didn't
     */
    bool ChangeIdent(const std::string& newident);

    /** Change a users realname field.
     * @param real The user's new real name
     * @return True if the change succeeded, false if otherwise
     */
    bool ChangeRealName(const std::string& real);

    /** Change a user's nick
     * @param newnick The new nick. If equal to the users uuid, the nick change always succeeds.
     * @param newts The time at which this nick change happened.
     * @return True if the change succeeded
     */
    bool ChangeNick(const std::string& newnick, time_t newts = 0);

    /** Remove this user from all channels they are on, and delete any that are now empty.
     * This is used by QUIT, and will not send part messages!
     */
    void PurgeEmptyChannels();

    /** Default destructor
     */
    virtual ~User();
    CullResult cull() CXX11_OVERRIDE;

    /** @copydoc Serializable::Deserialize */
    bool Deserialize(Data& data) CXX11_OVERRIDE;

    /** @copydoc Serializable::Deserialize */
    bool Serialize(Serializable::Data& data) CXX11_OVERRIDE;
};

class CoreExport UserIOHandler : public StreamSocket {
  private:
    size_t checked_until;
  public:
    LocalUser* const user;
    UserIOHandler(LocalUser* me)
        : StreamSocket(StreamSocket::SS_USER)
        , checked_until(0)
        , user(me) {
    }
    void OnDataReady() CXX11_OVERRIDE;
    bool OnSetEndPoint(const irc::sockets::sockaddrs& local,
                       const irc::sockets::sockaddrs& remote) CXX11_OVERRIDE;
    void OnError(BufferedSocketError error) CXX11_OVERRIDE;

    /** Adds to the user's write buffer.
     * You may add any amount of text up to this users sendq value, if you exceed the
     * sendq value, the user will be removed, and further buffer adds will be dropped.
     * @param data The data to add to the write buffer
     */
    void AddWriteBuf(const std::string &data);

    /** Swaps the internals of this UserIOHandler with another one.
     * @param other A UserIOHandler to swap internals with.
     */
    void SwapInternals(UserIOHandler& other);
};

class CoreExport LocalUser : public User,
    public insp::intrusive_list_node<LocalUser> {
    /** Add a serialized message to the send queue of the user.
     * @param serialized Bytes to add.
     */
    void Write(const ClientProtocol::SerializedMessage& serialized);

    /** Send a protocol event to the user, consisting of one or more messages.
     * @param protoev Event to send, may contain any number of messages.
     * @param msglist Message list used temporarily internally to pass to hooks and store messages
     * before Write().
     */
    void Send(ClientProtocol::Event& protoev, ClientProtocol::MessageList& msglist);

    /** Message list, can be passed to the two parameter Send().
     */
    static ClientProtocol::MessageList sendmsglist;

  public:
    LocalUser(int fd, irc::sockets::sockaddrs* client,
              irc::sockets::sockaddrs* server);
    LocalUser(int fd, const std::string& uuid, Serializable::Data& data);

    CullResult cull() CXX11_OVERRIDE;

    UserIOHandler eh;

    /** Serializer to use when communicating with the user
     */
    ClientProtocol::Serializer* serializer;

    /** Stats counter for bytes inbound
     */
    unsigned int bytes_in;

    /** Stats counter for bytes outbound
     */
    unsigned int bytes_out;

    /** Stats counter for commands inbound
     */
    unsigned int cmds_in;

    /** Stats counter for commands outbound
     */
    unsigned int cmds_out;

    /** Password specified by the user when they registered (if any).
     * This is stored even if the \<connect> block doesnt need a password, so that
     * modules may check it.
     */
    std::string password;

    /** Contains a pointer to the connect class a user is on from
     */
    reference<ConnectClass> MyClass;

    /** Get the connect class which this user belongs to.
     * @return A pointer to this user's connect class.
     */
    ConnectClass* GetClass() const {
        return MyClass;
    }

    /** Call this method to find the matching \<connect> for a user, and to check them against it.
     */
    void CheckClass(bool clone_count = true);

    /** Server address and port that this user is connected to.
     */
    irc::sockets::sockaddrs server_sa;

    /** Recursion fix: user is out of SendQ and will be quit as soon as possible.
     * This can't be handled normally because QuitUser itself calls Write on other
     * users, which could trigger their SendQ to overrun.
     */
    unsigned int quitting_sendq:1;

    /** has the user responded to their previous ping?
     */
    unsigned int lastping:1;

    /** This is true if the user matched an exception (E-line). It is used to save time on ban checks.
     */
    unsigned int exempt:1;

    /** The time at which this user should be pinged next. */
    time_t nextping;

    /** Time that the connection last sent a message, used to calculate idle time
     */
    time_t idle_lastmsg;

    /** This value contains how far into the penalty threshold the user is.
     * This is used either to enable fake lag or for excess flood quits
     */
    unsigned int CommandFloodPenalty;

    already_sent_t already_sent;

    /** Check if the user matches a G- or K-line, and disconnect them if they do.
     * @param doZline True if Z-lines should be checked (if IP has changed since initial connect)
     * Returns true if the user matched a ban, false else.
     */
    bool CheckLines(bool doZline = false);

    /** Use this method to fully connect a user.
     * This will send the message of the day, check G/K/E-lines, etc.
     */
    void FullConnect();

    /** Set the connect class to which this user belongs to.
     * @param explicit_name Set this string to tie the user to a specific class name. Otherwise, the class is fitted by checking \<connect> tags from the configuration file.
     */
    void SetClass(const std::string &explicit_name = "");

    /** @copydoc User::SetClientIP */
    void SetClientIP(const irc::sockets::sockaddrs& sa) CXX11_OVERRIDE;

    DEPRECATED_METHOD(bool SetClientIP(const std::string& address) CXX11_OVERRIDE);

    /** Send a NOTICE message from the local server to the user.
     * The message will be sent even if the user is connected to a remote server.
     * @param text Text to send
     */
    void WriteRemoteNotice(const std::string& text) CXX11_OVERRIDE;

    /** Returns true or false for if a user can execute a privileged oper command.
     * This is done by looking up their oper type from User::oper, then referencing
     * this to their oper classes and checking the commands they can execute.
     * @param command A command (should be all CAPS)
     * @return True if this user can execute the command
     */
    bool HasCommandPermission(const std::string& command) CXX11_OVERRIDE;

    /** Returns true if a user has a given permission.
     * This is used to check whether or not users may perform certain actions which admins may not wish to give to
     * all operators, yet are not commands. An example might be oper override, mass messaging (/notice $*), etc.
     *
     * @param privstr The priv to check, e.g. "users/override/topic". These are loaded free-form from the config file.
     * @return True if this user has the permission in question.
     */
    bool HasPrivPermission(const std::string& privstr) CXX11_OVERRIDE;

    /** Returns true or false if a user can set a privileged user or channel mode.
     * This is done by looking up their oper type from User::oper, then referencing
     * this to their oper classes, and checking the modes they can set.
     * @param mh Mode to check
     * @return True if the user can set or unset this mode.
     */
    bool HasModePermission(const ModeHandler* mh) const CXX11_OVERRIDE;

    /** @copydoc User::HasSnomaskPermission */
    bool HasSnomaskPermission(char chr) const CXX11_OVERRIDE;

    /** Change nick to uuid, unset REG_NICK and send a nickname overruled numeric.
     * This is called when another user (either local or remote) needs the nick of this user and this user
     * isn't registered.
     */
    void OverruleNick();

    /** Send a protocol event to the user, consisting of one or more messages.
     * @param protoev Event to send, may contain any number of messages.
     */
    void Send(ClientProtocol::Event& protoev);

    /** Send a single message to the user.
     * @param protoevprov Protocol event provider.
     * @param msg Message to send.
     */
    void Send(ClientProtocol::EventProvider& protoevprov,
              ClientProtocol::Message& msg);

    /** @copydoc Serializable::Deserialize */
    bool Deserialize(Data& data) CXX11_OVERRIDE;

    /** @copydoc Serializable::Deserialize */
    bool Serialize(Serializable::Data& data) CXX11_OVERRIDE;
};

class RemoteUser : public User {
  public:
    RemoteUser(const std::string& uid, Server* srv) : User(uid, srv,
                USERTYPE_REMOTE) {
    }
};

class CoreExport FakeUser : public User {
  public:
    FakeUser(const std::string& uid, Server* srv)
        : User(uid, srv, USERTYPE_SERVER) {
        nick = srv->GetName();
    }

    FakeUser(const std::string& uid, const std::string& sname,
             const std::string& sdesc)
        : User(uid, new Server(uid, sname, sdesc), USERTYPE_SERVER) {
        nick = sname;
    }

    CullResult cull() CXX11_OVERRIDE;
    const std::string& GetFullHost() CXX11_OVERRIDE;
    const std::string& GetFullRealHost() CXX11_OVERRIDE;
};

/* Faster than dynamic_cast */
/** Is a local user */
inline LocalUser* IS_LOCAL(User* u) {
    return (u != NULL
            && u->usertype == USERTYPE_LOCAL) ? static_cast<LocalUser*>(u) : NULL;
}
/** Is a remote user */
inline RemoteUser* IS_REMOTE(User* u) {
    return (u != NULL
            && u->usertype == USERTYPE_REMOTE) ? static_cast<RemoteUser*>(u) : NULL;
}
/** Is a server fakeuser */
inline FakeUser* IS_SERVER(User* u) {
    return (u != NULL
            && u->usertype == USERTYPE_SERVER) ? static_cast<FakeUser*>(u) : NULL;
}

inline bool User::IsModeSet(const ModeHandler* mh) const {
    return ((mh->GetId() != ModeParser::MODEID_MAX) && (modes[mh->GetId()]));
}

inline bool User::IsModeSet(UserModeReference& moderef) const {
    if (!moderef) {
        return false;
    }
    return IsModeSet(*moderef);
}

inline void User::SetMode(ModeHandler* mh, bool value) {
    if (mh && mh->GetId() != ModeParser::MODEID_MAX) {
        modes[mh->GetId()] = value;
    }
}
