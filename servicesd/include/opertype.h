/*
 *
 * (C) 2008-2011 Robin Burchell <w00t@inspircd.org>
 * (C) 2008-2023 Anope Team <team@anope.org>
 *
 * Please read COPYING and README for further details.
 */

#ifndef OPERTYPE_H
#define OPERTYPE_H

#include "services.h"
#include "account.h"

/* A services operator. Usually made by the configuration file, but not always.
 * NickAlias::Find(name)->nc->o == this
 */
struct CoreExport Oper {
    /* The oper's nick */
    Anope::string name;
    /* The type of operator this operator is */
    OperType *ot;
    /* Whether the user must be an IRC operator (umode +o) to be considered a services operator */
    bool require_oper;
    Anope::string password;
    Anope::string certfp;
    /* Hosts allowed to use this operator block */
    std::vector<Anope::string> hosts;
    Anope::string vhost;

    Oper(const Anope::string &n, OperType *o);
    virtual ~Oper();

    static std::vector<Oper *> opers;

    /** Find an oper block by name
     * @param name The name
     * @return the oper block
     */
    static Oper *Find(const Anope::string &name);
};

class CoreExport OperType {
  private:
    /** The name of this opertype, e.g. "sra".
     */
    Anope::string name;

    /** Privs that this opertype may use, e.g. 'users/auspex'.
     * This *must* be std::list, see commands comment for details.
     */
    std::list<Anope::string> privs;

    /** Commands this user may execute, e.g:
     * botserv/set/ *, botserv/set/private, botserv/ *
     * et cetera.
     *
     * This *must* be std::list, not std::map, because
     * we support full globbing here. This shouldn't be a problem
     * as we don't invoke it often.
     */
    std::list<Anope::string> commands;

    /** Set of opertypes we inherit from
     */
    std::set<OperType *> inheritances;
  public:
    /** Modes to set when someone identifies using this opertype
     */
    Anope::string modes;

    /** Find an oper type by name
     * @param name The name
     * @return The oper type
     */
    static OperType *Find(const Anope::string &name);

    /** Create a new opertype of the given name.
     * @param nname The opertype name, e.g. "sra".
     */
    OperType(const Anope::string &nname);

    /** Check whether this opertype has access to run the given command string.
     * @param cmdstr The string to check, e.g. botserv/set/private.
     * @return True if this opertype may run the specified command, false otherwise.
     */
    bool HasCommand(const Anope::string &cmdstr) const;

    /** Check whether this opertype has access to the given special permission.
     * @param privstr The priv to check for, e.g. users/auspex.
     * @return True if this opertype has the specified priv, false otherwise.
     */
    bool HasPriv(const Anope::string &privstr) const;

    /** Add the specified command to this opertype.
     * @param cmdstr The command mask to grant this opertype access to, e.g: nickserv/ *, chanserv/set/ *, botserv/set/private.
     */
    void AddCommand(const Anope::string &cmdstr);

    /** Add the specified priv mask to this opertype.
     * @param privstr The specified mask of privs to grant this opertype access to,  e.g. users/auspex, users/ *, etc.
     */
    void AddPriv(const Anope::string &privstr);

    /** Returns the name of this opertype.
     */
    const Anope::string &GetName() const;

    /** Make this opertype inherit commands and privs from another opertype
     * @param ot The opertype to inherit from
     */
    void Inherits(OperType *ot);

    /** Gets the icommands for this opertype
     * @return A list of commands
     */
    const std::list<Anope::string> GetCommands() const;

    /** Gets the privileges for this opertype
     * @return A list of privileges
     */
    const std::list<Anope::string> GetPrivs() const;
};

#endif // OPERTYPE_H
