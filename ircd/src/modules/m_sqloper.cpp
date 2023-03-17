/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2018 Dylan Frank <b00mx0r@aureus.pw>
 *   Copyright (C) 2013-2014, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2017-2018, 2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2007-2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2007 Craig Edwards <brain@inspircd.org>
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
#include "modules/sql.h"

class OperQuery : public SQL::Query {
  public:
    // This variable will store all the OPER blocks from the DB
    std::vector<std::string>& my_blocks;
    /** We want to store the username and password if this is called during an /OPER, as we're responsible for /OPER post-DB fetch
     *  Note: uid will be empty if this DB update was not called as a result of a user command (i.e. /REHASH)
     */
    const std::string uid, username, password;
    OperQuery(Module* me, std::vector<std::string>& mb, const std::string& u,
              const std::string& un, const std::string& pw)
        : SQL::Query(me)
        , my_blocks(mb)
        , uid(u)
        , username(un)
        , password(pw) {
    }
    OperQuery(Module* me, std::vector<std::string>& mb)
        : SQL::Query(me)
        , my_blocks(mb) {
    }

    void OnResult(SQL::Result& res) CXX11_OVERRIDE {
        ServerConfig::OperIndex& oper_blocks = ServerInstance->Config->oper_blocks;

        // Remove our previous blocks from oper_blocks for a clean update
        for (std::vector<std::string>::const_iterator i = my_blocks.begin(); i != my_blocks.end(); ++i) {
            oper_blocks.erase(*i);
        }
        my_blocks.clear();

        SQL::Row row;
        // Iterate through DB results to create oper blocks from sqloper rows
        while (res.GetRow(row)) {
            std::vector<std::string> cols;
            res.GetCols(cols);

            // Create the oper tag as if we were the conf file.
            ConfigItems* items;
            reference<ConfigTag> tag = ConfigTag::create("oper", MODNAME, 0, items);

            /** Iterate through each column in the SQLOpers table. An infinite number of fields can be specified.
             *  Column 'x' with cell value 'y' will be the same as x=y in an OPER block in opers.conf.
             */
            for (unsigned int i=0; i < cols.size(); ++i) {
                if (!row[i].IsNull()) {
                    (*items)[cols[i]] = row[i];
                }
            }
            const std::string name = tag->getString("name");

            // Skip both duplicate sqloper blocks and sqloper blocks that attempt to override conf blocks.
            if (oper_blocks.find(name) != oper_blocks.end()) {
                continue;
            }

            const std::string type = tag->getString("type");
            ServerConfig::OperIndex::iterator tblk = ServerInstance->Config->OperTypes.find(
                        type);
            if (tblk == ServerInstance->Config->OperTypes.end()) {
                ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT,
                                          "Sqloper block " + name + " has missing type " + type);
                ServerInstance->SNO->WriteGlobalSno('a',
                                                    "m_sqloper: Oper block %s has missing type %s", name.c_str(), type.c_str());
                continue;
            }

            OperInfo* ifo = new OperInfo(type);

            ifo->type_block = tblk->second->type_block;
            ifo->oper_block = tag;
            ifo->class_blocks.assign(tblk->second->class_blocks.begin(),
                                     tblk->second->class_blocks.end());
            oper_blocks[name] = ifo;
            my_blocks.push_back(name);
            row.clear();
        }

        // If this was done as a result of /OPER and not a config read
        if (!uid.empty()) {
            // Now that we've updated the DB, call any other /OPER hooks and then call /OPER
            OperExec();
        }
    }

    void OnError(SQL::Error& error) CXX11_OVERRIDE {
        ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "query failed (%s)", error.ToString());
        ServerInstance->SNO->WriteGlobalSno('a', "m_sqloper: Failed to update blocks from database");
        if (!uid.empty()) {
            // Fallback. We don't want to block a netadmin from /OPER
            OperExec();
        }
    }

    // Call /oper after placing all blocks from the SQL table into the config->oper_blocks list.
    void OperExec() {
        User* user = ServerInstance->FindNick(uid);
        LocalUser* localuser = IS_LOCAL(user);
        // This should never be true
        if (!localuser) {
            return;
        }

        Command* oper_command = ServerInstance->Parser.GetHandler("OPER");

        if (oper_command) {
            CommandBase::Params params;
            params.push_back(username);
            params.push_back(password);

            // Begin callback to other modules (i.e. sslinfo) now that we completed the DB fetch
            ModResult MOD_RESULT;

            std::string origin = "OPER";
            FIRST_MOD_RESULT(OnPreCommand, MOD_RESULT, (origin, params, localuser, true));
            if (MOD_RESULT == MOD_RES_DENY) {
                return;
            }

            // Now handle /OPER.
            ClientProtocol::TagMap tags;
            oper_command->Handle(user, CommandBase::Params(params, tags));
        } else {
            ServerInstance->Logs->Log(MODNAME, LOG_SPARSE,
                                      "BUG: WHAT?! Why do we have no OPER command?!");
        }
    }
};

class ModuleSQLOper : public Module {
    // Whether OperQuery is running
    bool active;
    std::string query;
    // Stores oper blocks from DB
    std::vector<std::string> my_blocks;
    dynamic_reference<SQL::Provider> SQL;

  public:
    ModuleSQLOper()
        : active(false)
        , SQL(this, "SQL") {
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        // Clear list of our blocks, as ConfigReader just wiped them anyway
        my_blocks.clear();

        ConfigTag* tag = ServerInstance->Config->ConfValue("sqloper");

        std::string dbid = tag->getString("dbid");
        if (dbid.empty()) {
            SQL.SetProvider("SQL");
        } else {
            SQL.SetProvider("SQL/" + dbid);
        }

        query = tag->getString("query", "SELECT * FROM ircd_opers WHERE active=1;", 1);
        // Update sqloper list from the database.
        GetOperBlocks();
    }

    ~ModuleSQLOper() {
        // Remove all oper blocks that were from the DB
        for (std::vector<std::string>::const_iterator i = my_blocks.begin();
                i != my_blocks.end(); ++i) {
            ServerInstance->Config->oper_blocks.erase(*i);
        }
    }

    ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters,
                           LocalUser* user, bool validated) CXX11_OVERRIDE {
        // If we are not in the middle of an existing /OPER and someone is trying to oper-up
        if (validated && command == "OPER" && parameters.size() >= 2 && !active) {
            if (SQL) {
                GetOperBlocks(user->uuid, parameters[0], parameters[1]);
                /** We need to reload oper blocks from the DB before other
                 *  hooks can run (i.e. sslinfo). We will re-call /OPER later.
                 */
                return MOD_RES_DENY;
            }
            ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "database not present");
        } else if (active) {
            active = false;
        }
        // There is either no DB or we successfully reloaded oper blocks
        return MOD_RES_PASSTHRU;
    }

    // The one w/o params is for non-/OPER DB updates, such as a rehash.
    void GetOperBlocks() {
        SQL->Submit(new OperQuery(this, my_blocks), query);
    }
    void GetOperBlocks(const std::string u, const std::string& un,
                       const std::string& pw) {
        active = true;
        // Call to SQL query to fetch oper list from SQL table.
        SQL->Submit(new OperQuery(this, my_blocks, u, un, pw), query);
    }

    void Prioritize() CXX11_OVERRIDE {
        /** Run before other /OPER hooks that expect populated blocks, i.e. sslinfo or a TOTP module.
         *  We issue a DENY first, and will re-run OnPreCommand later to trigger the other hooks post-DB update.
         */
        ServerInstance->Modules.SetPriority(this, I_OnPreCommand, PRIORITY_FIRST);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows server operators to be authenticated against an SQL table.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleSQLOper)
