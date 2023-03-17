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

#ifndef SERVERS_H
#define SERVERS_H

#include "services.h"
#include "anope.h"
#include "extensible.h"

/* Anope. We are at the top of the server tree, our uplink is
 * almost always me->GetLinks()[0]. We never have an uplink. */
extern CoreExport Server *Me;

namespace Servers {
/* Gets our uplink. Note we don't actually have an "uplink", this is just
 * the only server whose uplink *is* Me that is not a juped server.
 * @return Our uplink, or NULL if not uplinked to anything
 */
extern CoreExport Server* GetUplink();

/* Server maps by name and id */
extern CoreExport Anope::map<Server *> ByName;
extern CoreExport Anope::map<Server *> ByID;

/* CAPAB/PROTOCTL given by the uplink */
extern CoreExport std::set<Anope::string> Capab;
}

/** Class representing a server
 */
class CoreExport Server : public Extensible {
  private:
    /* Server name */
    Anope::string name;
    /* Hops between services and server */
    unsigned int hops;
    /* Server description */
    Anope::string description;
    /* Server ID */
    Anope::string sid;
    /* Links for this server */
    std::vector<Server *> links;
    /* Uplink for this server */
    Server *uplink;
    /* Server is syncing */
    bool syncing;
    /* The server is juped */
    bool juped;
    /* The server is about to quit */
    bool quitting;
    /* Reason this server was quit */
    Anope::string quit_reason;

  public:
    /** Constructor
     * @param uplink The uplink this server is from, is only NULL when creating Me
     * @param name The server name
     * @param hops Hops from services server
     * @param description Server rdescription
     * @param sid Server sid/numeric
     * @param jupe If the server is juped
     */
    Server(Server *uplink, const Anope::string &name, unsigned hops,
           const Anope::string &description, const Anope::string &sid = "",
           bool jupe = false);

  private:
    /** Destructor
     */
    ~Server();

  public:
    /* Number of users on the server */
    unsigned users;

    /** Delete this server with a reason
     * @param reason The reason
     */
    void Delete(const Anope::string &reason);

    /** Get the name for this server
     * @return The name
     */
    const Anope::string &GetName() const;

    /** Get the number of hops this server is from services
     * @return Number of hops
     */
    unsigned GetHops() const;

    /** Set the server description
     * @param desc The new description
     */
    void SetDescription(const Anope::string &desc);

    /** Get the server description
     * @return The server description
     */
    const Anope::string &GetDescription() const;

    /** Change this servers SID
     * @param sid The new SID
     */
    void SetSID(const Anope::string &sid);

    /** Get the server numeric/SID, else the server name
     * @return The numeric/SID
     */
    const Anope::string &GetSID() const;

    /** Retrieves the reason this server is quitting
     */
    const Anope::string &GetQuitReason() const;

    /** Get the list of links this server has, or NULL if it has none
     * @return A list of servers
     */
    const std::vector<Server *> &GetLinks() const;

    /** Get the uplink server for this server, if this is our uplink will be Me
     * @return The servers uplink
     */
    Server *GetUplink();

    /** Adds a link to this server
     * @param s The linking server
     */
    void AddLink(Server *s);

    /** Delinks a server from this server
     * @param s The server
     */
    void DelLink(Server *s);

    /** Finish syncing this server and optionally all links to it
     * @param sync_links True to sync the links for this server too (if any)
     */
    void Sync(bool sync_links);

    /** Check if this server is synced
     * @return true or false
     */
    bool IsSynced() const;

    /** Unsync the server. Only used for Me->Unsync()
     */
    void Unsync();

    /** Check if this server is ULined
     * @return true or false
     */
    bool IsULined() const;

    /** Check if this server is juped (a pseudoserver other than us)
     * @return true if this server is a juped server
     */
    bool IsJuped() const;

    /** Check if the server is quitting
     * @return true if this server is quitting.
     */
    bool IsQuitting() const;

    /** Send a message to all users on this server
     * @param source The source of the message
     * @param message The message
     */
    void Notice(BotInfo *source, const Anope::string &message);

    /** Find a server
     * @param name The name or SID/numeric
     * @param name_only set to true to only look up by name, not SID
     * @return The server
     */
    static Server *Find(const Anope::string &name, bool name_only = false);
};

#endif // SERVERS_H
