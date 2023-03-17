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

#ifndef ACCOUNT_H
#define ACCOUNT_H

#include "extensible.h"
#include "serialize.h"
#include "anope.h"
#include "memo.h"
#include "base.h"

typedef Anope::hash_map<NickAlias *> nickalias_map;
typedef Anope::hash_map<NickCore *> nickcore_map;
typedef TR1NS::unordered_map<uint64_t, NickCore *> nickcoreid_map;

extern CoreExport Serialize::Checker<nickalias_map> NickAliasList;
extern CoreExport Serialize::Checker<nickcore_map> NickCoreList;
extern CoreExport nickcoreid_map NickCoreIdList;

/* A registered nickname.
 * It matters that Base is here before Extensible (it is inherited by Serializable)
 */
class CoreExport NickAlias : public Serializable, public Extensible {
    Anope::string vhost_ident, vhost_host, vhost_creator;
    time_t vhost_created;

  public:
    Anope::string nick;
    Anope::string last_quit;
    Anope::string last_realname;
    /* Last usermask this nick was seen on, eg user@host */
    Anope::string last_usermask;
    /* Last uncloaked usermask, requires nickserv/auspex to see */
    Anope::string last_realhost;
    time_t time_registered;
    time_t last_seen;
    /* Account this nick is tied to. Multiple nicks can be tied to a single account. */
    Serialize::Reference<NickCore> nc;

    /** Constructor
     * @param nickname The nick
     * @param nickcore The nickcore for this nick
     */
    NickAlias(const Anope::string &nickname, NickCore *nickcore);
    ~NickAlias();

    void Serialize(Serialize::Data &data) const anope_override;
    static Serializable* Unserialize(Serializable *obj, Serialize::Data &);

    /** Set a vhost for the user
     * @param ident The ident
     * @param host The host
     * @param creator Who created the vhost
     * @param time When the vhost was created
     */
    void SetVhost(const Anope::string &ident, const Anope::string &host,
                  const Anope::string &creator, time_t created = Anope::CurTime);

    /** Remove a users vhost
     **/
    void RemoveVhost();

    /** Check if the user has a vhost
     * @return true or false
     */
    bool HasVhost() const;

    /** Retrieve the vhost ident
     * @return the ident
     */
    const Anope::string &GetVhostIdent() const;

    /** Retrieve the vhost host
     * @return the host
     */
    const Anope::string &GetVhostHost() const;

    /** Retrieve the vhost creator
     * @return the creator
     */
    const Anope::string &GetVhostCreator() const;

    /** Retrieve when the vhost was created
     * @return the time it was created
     */
    time_t GetVhostCreated() const;

    /** Finds a registered nick
     * @param nick The nick to lookup
     * @return the nick, if found
     */
    static NickAlias *Find(const Anope::string &nick);
};

/* A registered account. Each account must have a NickAlias with the same nick as the
 * account's display.
 * It matters that Base is here before Extensible (it is inherited by Serializable)
 */
class CoreExport NickCore : public Serializable, public Extensible {
    /* Channels which reference this core in some way (this is on their access list, akick list, is founder, successor, etc) */
    Serialize::Checker<std::map<ChannelInfo *, int> > chanaccess;
    /* Unique identifier for the account. */
    uint64_t id;
  public:
    /* Name of the account. Find(display)->nc == this. */
    Anope::string display;
    /* User password in form of hashm:data */
    Anope::string pass;
    Anope::string email;
    /* Locale name of the language of the user. Empty means default language */
    Anope::string language;
    /* Access list, contains user@host masks of users who get certain privileges based
     * on if NI_SECURE is set and what (if any) kill protection is enabled. */
    std::vector<Anope::string> access;
    MemoInfo memos;
    std::map<Anope::string, Anope::string> last_modes;

    /* Nicknames registered that are grouped to this account.
     * for n in aliases, n->nc == this.
     */
    Serialize::Checker<std::vector<NickAlias *> > aliases;

    /* Set if this user is a services operator. o->ot must exist. */
    Oper *o;

    /* Unsaved data */

    /* Number of channels registered by this account */
    uint16_t channelcount;
    /* Last time an email was sent to this user */
    time_t lastmail;
    /* Users online now logged into this account */
    std::list<User *> users;

    /** Constructor
     * @param display The display nick
     * @param id The account id
     */
    NickCore(const Anope::string &nickdisplay, uint64_t nickid = 0);
    ~NickCore();

    void Serialize(Serialize::Data &data) const anope_override;
    static Serializable* Unserialize(Serializable *obj, Serialize::Data &);

    /** Changes the display for this account
     * @param na The new display, must be grouped to this account.
     */
    void SetDisplay(const NickAlias *na);

    /** Checks whether this account is a services oper or not.
     * @return True if this account is a services oper, false otherwise.
     */
    virtual bool IsServicesOper() const;

    /** Add an entry to the nick's access list
     *
     * @param entry The nick!ident@host entry to add to the access list
     *
     * Adds a new entry into the access list.
     */
    void AddAccess(const Anope::string &entry);

    /** Get an entry from the nick's access list by index
     *
     * @param entry Index in the access list vector to retrieve
     * @return The access list entry of the given index if within bounds, an empty string if the vector is empty or the index is out of bounds
     *
     * Retrieves an entry from the access list corresponding to the given index.
     */
    Anope::string GetAccess(unsigned entry) const;

    /** Get the number of entries on the access list for this account.
     */
    unsigned GetAccessCount() const;

    /** Retrieves the account id for this user */
    uint64_t GetId();

    /** Find an entry in the nick's access list
     *
     * @param entry The nick!ident@host entry to search for
     * @return True if the entry is found in the access list, false otherwise
     *
     * Search for an entry within the access list.
     */
    bool FindAccess(const Anope::string &entry);

    /** Erase an entry from the nick's access list
     *
     * @param entry The nick!ident@host entry to remove
     *
     * Removes the specified access list entry from the access list.
     */
    void EraseAccess(const Anope::string &entry);

    /** Clears the entire nick's access list
     *
     * Deletes all the memory allocated in the access list vector and then clears the vector.
     */
    void ClearAccess();

    /** Is the given user on this accounts access list?
     *
     * @param u The user
     *
     * @return true if the user is on the access list
     */
    bool IsOnAccess(const User *u) const;

    /** Finds an account
     * @param nick The account name to find
     * @return The account, if it exists
     */
    static NickCore* Find(const Anope::string &nick);

    void AddChannelReference(ChannelInfo *ci);
    void RemoveChannelReference(ChannelInfo *ci);
    void GetChannelReferences(std::deque<ChannelInfo *> &queue);
};

/* A request to check if an account/password is valid. These can exist for
 * extended periods due to the time some authentication modules take.
 */
class CoreExport IdentifyRequest {
    /* Owner of this request, used to cleanup requests if a module is unloaded
     * while a request us pending */
    Module *owner;
    Anope::string account;
    Anope::string password;

    std::set<Module *> holds;
    bool dispatched;
    bool success;

    static std::set<IdentifyRequest *> Requests;

  protected:
    IdentifyRequest(Module *o, const Anope::string &acc, const Anope::string &pass);
    virtual ~IdentifyRequest();

  public:
    /* One of these is called when the request goes through */
    virtual void OnSuccess() = 0;
    virtual void OnFail() = 0;

    Module *GetOwner() const {
        return owner;
    }
    const Anope::string &GetAccount() const {
        return account;
    }
    const Anope::string &GetPassword() const {
        return password;
    }

    /* Holds this request. When a request is held it must be Released later
     * for the request to complete. Multiple modules may hold a request at any time,
     * but the request is not complete until every module has released it. If you do not
     * require holding this (eg, your password check is done in this thread and immediately)
     * then you don't need to hold the request before calling `Success()`.
     * @param m The module holding this request
     */
    void Hold(Module *m);

    /** Releases a held request
     * @param m The module releasing the hold
     */
    void Release(Module *m);

    /** Called by modules when this IdentifyRequest has succeeded.
     * If this request is behind held it must still be Released after calling this.
     * @param m The module confirming authentication
     */
    void Success(Module *m);

    /** Used to either finalize this request or marks
     * it as dispatched and begins waiting for the module(s)
     * that have holds to finish.
     */
    void Dispatch();

    static void ModuleUnload(Module *m);
};

#endif // ACCOUNT_H
