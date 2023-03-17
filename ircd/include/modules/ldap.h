/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2015 Adam <Adam@anope.org>
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

typedef int LDAPQuery;

class LDAPException : public ModuleException {
  public:
    LDAPException(const std::string& reason) : ModuleException(reason) { }

    virtual ~LDAPException() throw() { }
};

struct LDAPModification {
    enum LDAPOperation {
        LDAP_ADD,
        LDAP_DEL,
        LDAP_REPLACE
    };

    LDAPOperation op;
    std::string name;
    std::vector<std::string> values;
};

typedef std::vector<LDAPModification> LDAPMods;

struct LDAPAttributes : public
    std::map<std::string, std::vector<std::string> > {
    size_t size(const std::string& attr) const {
        const std::vector<std::string>& array = this->getArray(attr);
        return array.size();
    }

    const std::vector<std::string> keys() const {
        std::vector<std::string> k;
        for (const_iterator it = this->begin(), it_end = this->end(); it != it_end;
                ++it) {
            k.push_back(it->first);
        }
        return k;
    }

    const std::string& get(const std::string& attr) const {
        const std::vector<std::string>& array = this->getArray(attr);
        if (array.empty()) {
            throw LDAPException("Empty attribute " + attr + " in LDAPResult::get");
        }
        return array[0];
    }

    const std::vector<std::string>& getArray(const std::string& attr) const {
        const_iterator it = this->find(attr);
        if (it == this->end()) {
            throw LDAPException("Unknown attribute " + attr + " in LDAPResult::getArray");
        }
        return it->second;
    }
};

enum QueryType {
    QUERY_UNKNOWN,
    QUERY_BIND,
    QUERY_SEARCH,
    QUERY_ADD,
    QUERY_DELETE,
    QUERY_MODIFY,
    QUERY_COMPARE
};

struct LDAPResult {
    std::vector<LDAPAttributes> messages;
    std::string error;

    QueryType type;
    LDAPQuery id;

    LDAPResult()
        : type(QUERY_UNKNOWN), id(-1) {
    }

    size_t size() const {
        return this->messages.size();
    }

    bool empty() const {
        return this->messages.empty();
    }

    const LDAPAttributes& get(size_t sz) const {
        if (sz >= this->messages.size()) {
            throw LDAPException("Index out of range");
        }
        return this->messages[sz];
    }

    const std::string& getError() const {
        return this->error;
    }
};

class LDAPInterface {
  public:
    ModuleRef creator;

    LDAPInterface(Module* m) : creator(m) { }
    virtual ~LDAPInterface() { }

    virtual void OnResult(const LDAPResult& r) = 0;
    virtual void OnError(const LDAPResult& err) = 0;
};

class LDAPProvider : public DataProvider {
  public:
    LDAPProvider(Module* Creator, const std::string& Name)
        : DataProvider(Creator, Name) { }

    /** Attempt to bind to the LDAP server as a manager
     * @param i The LDAPInterface the result is sent to
     */
    virtual void BindAsManager(LDAPInterface* i) = 0;

    /** Bind to LDAP
     * @param i The LDAPInterface the result is sent to
     * @param who The binddn
     * @param pass The password
     */
    virtual void Bind(LDAPInterface* i, const std::string& who,
                      const std::string& pass) = 0;

    /** Search ldap for the specified filter
     * @param i The LDAPInterface the result is sent to
     * @param base The base DN to search
     * @param filter The filter to apply
     */
    virtual void Search(LDAPInterface* i, const std::string& base,
                        const std::string& filter) = 0;

    /** Add an entry to LDAP
     * @param i The LDAPInterface the result is sent to
     * @param dn The dn of the entry to add
     * @param attributes The attributes
     */
    virtual void Add(LDAPInterface* i, const std::string& dn,
                     LDAPMods& attributes) = 0;

    /** Delete an entry from LDAP
     * @param i The LDAPInterface the result is sent to
     * @param dn The dn of the entry to delete
     */
    virtual void Del(LDAPInterface* i, const std::string& dn) = 0;

    /** Modify an existing entry in LDAP
     * @param i The LDAPInterface the result is sent to
     * @param base The base DN to modify
     * @param attributes The attributes to modify
     */
    virtual void Modify(LDAPInterface* i, const std::string& base,
                        LDAPMods& attributes) = 0;

    /** Compare an attribute in LDAP with our value
     * @param i The LDAPInterface the result is sent to
     * @param dn DN to use for comparing
     * @param attr Attr of DN to compare with
     * @param val value to compare attr of dn
     */
    virtual void Compare(LDAPInterface* i, const std::string& dn,
                         const std::string& attr, const std::string& val) = 0;
};
