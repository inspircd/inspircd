/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2018 Dylan Frank <b00mx0r@aureus.pw>
 *   Copyright (C) 2014, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2017-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2007-2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Craig Edwards <brain@inspircd.org>
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

class OperQuery final
	: public SQL::Query
{
public:
	// This variable will store all the OPER blocks from the DB
	std::vector<std::string>& my_blocks;
	/** We want to store the username and password if this is called during an /OPER, as we're responsible for /OPER post-DB fetch
	 *  Note: uid will be empty if this DB update was not called as a result of a user command (i.e. /REHASH)
	 */
	const std::string uid, username, password;
	OperQuery(Module* me, std::vector<std::string>& mb, const std::string& u, const std::string& un, const std::string& pw)
		: SQL::Query(me)
		, my_blocks(mb)
		, uid(u)
		, username(un)
		, password(pw)
	{
	}
	OperQuery(Module* me, std::vector<std::string>& mb)
		: SQL::Query(me)
		, my_blocks(mb)
	{
	}

	void OnResult(SQL::Result& res) override
	{
		// Remove our previous blocks from the core for a clean update
		for (const auto& block : my_blocks)
			ServerInstance->Config->OperAccounts.erase(block);
		my_blocks.clear();

		SQL::Row row;
		// Iterate through DB results to create oper blocks from sqloper rows
		for (size_t rowidx = 1; res.GetRow(row); ++rowidx)
		{
			std::vector<std::string> cols;
			res.GetCols(cols);

			// Create the oper tag as if we were the conf file.
			auto tag = std::make_shared<ConfigTag>("oper", FilePosition("<" MODNAME ">" , rowidx, 0));

			/** Iterate through each column in the SQLOpers table. An infinite number of fields can be specified.
			 *  Column 'x' with cell value 'y' will be the same as x=y in an OPER block in opers.conf.
			 */
			for (unsigned int i=0; i < cols.size(); ++i)
			{
				if (row[i].has_value())
					tag->GetItems()[cols[i]] = *row[i];
			}
			const std::string name = tag->getString("name");

			// Skip both duplicate sqloper blocks and sqloper blocks that attempt to override conf blocks.
			if (ServerInstance->Config->OperAccounts.find(name) != ServerInstance->Config->OperAccounts.end())
				continue;

			const std::string type = tag->getString("type");
			auto tblk = ServerInstance->Config->OperTypes.find(type);
			if (tblk == ServerInstance->Config->OperTypes.end())
			{
				ServerInstance->Logs.Warning(MODNAME, "Sqloper block " + name + " has missing type " + type);
				ServerInstance->SNO.WriteGlobalSno('a', "m_sqloper: Oper block {} has missing type {}", name, type);
				continue;
			}

			ServerInstance->Config->OperAccounts[name] = std::make_shared<OperAccount>(name, tblk->second, tag);
			my_blocks.push_back(name);
			row.clear();
		}

		// If this was done as a result of /OPER and not a config read
		if (!uid.empty())
		{
			// Now that we've updated the DB, call any other /OPER hooks and then call /OPER
			OperExec();
		}
	}

	void OnError(const SQL::Error& error) override
	{
		ServerInstance->Logs.Warning(MODNAME, "query failed ({})", error.ToString());
		ServerInstance->SNO.WriteGlobalSno('a', "m_sqloper: Failed to update blocks from database");
		if (!uid.empty())
		{
			// Fallback. We don't want to block a netadmin from /OPER
			OperExec();
		}
	}

	// Call /oper after placing all blocks from the SQL table into the Config->OperAccounts list.
	void OperExec()
	{
		auto* user = ServerInstance->Users.Find<LocalUser>(uid);
		if (!user)
			return; // The user disconnected before the SQL query returned.

		Command* oper_command = ServerInstance->Parser.GetHandler("OPER");

		if (oper_command)
		{
			CommandBase::Params params;
			params.push_back(username);
			params.push_back(password);

			// Begin callback to other modules (i.e. sslinfo) now that we completed the DB fetch
			ModResult MOD_RESULT;

			std::string origin = "OPER";
			FIRST_MOD_RESULT(OnPreCommand, MOD_RESULT, (origin, params, user, true));
			if (MOD_RESULT == MOD_RES_DENY)
				return;

			// Now handle /OPER.
			ClientProtocol::TagMap tags;
			oper_command->Handle(user, CommandBase::Params(params, tags));
		}
		else
		{
			ServerInstance->Logs.Debug(MODNAME, "BUG: WHAT?! Why do we have no OPER command?!");
		}
	}
};

class ModuleSQLOper final
	: public Module
{
private:
	// Whether OperQuery is running
	bool active = false;
	std::string query;
	// Stores oper blocks from DB
	std::vector<std::string> my_blocks;
	dynamic_reference<SQL::Provider> SQL;

public:
	ModuleSQLOper()
		: Module(VF_VENDOR, "Allows server operators to be authenticated against an SQL table.")
		, SQL(this, "SQL")
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		// Clear list of our blocks, as ConfigReader just wiped them anyway
		my_blocks.clear();

		const auto& tag = ServerInstance->Config->ConfValue("sqloper");

		std::string dbid = tag->getString("dbid");
		if (dbid.empty())
			SQL.SetProvider("SQL");
		else
			SQL.SetProvider("SQL/" + dbid);

		query = tag->getString("query", "SELECT * FROM ircd_opers WHERE active=1;", 1);
		// Update sqloper list from the database.
		GetOperBlocks();
	}

	~ModuleSQLOper() override
	{
		// Remove all oper blocks that were from the DB
		for (const auto& block : my_blocks)
			ServerInstance->Config->OperAccounts.erase(block);
	}

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) override
	{
		// If we are not in the middle of an existing /OPER and someone is trying to oper-up
		if (validated && command == "OPER" && parameters.size() >= 2 && !active)
		{
			if (SQL)
			{
				GetOperBlocks(user->uuid, parameters[0], parameters[1]);
				/** We need to reload oper blocks from the DB before other
				 *  hooks can run (i.e. sslinfo). We will re-call /OPER later.
				 */
				return MOD_RES_DENY;
			}
			ServerInstance->Logs.Warning(MODNAME, "database not present");
		}
		else if (active)
		{
			active = false;
		}
		// There is either no DB or we successfully reloaded oper blocks
		return MOD_RES_PASSTHRU;
	}

	// The one w/o params is for non-/OPER DB updates, such as a rehash.
	void GetOperBlocks()
	{
		SQL->Submit(new OperQuery(this, my_blocks), query);
	}
	void GetOperBlocks(const std::string& u, const std::string& un, const std::string& pw)
	{
		active = true;
		// Call to SQL query to fetch oper list from SQL table.
		SQL->Submit(new OperQuery(this, my_blocks, u, un, pw), query);
	}

	void Prioritize() override
	{
		/** Run before other /OPER hooks that expect populated blocks, i.e. sslinfo or a TOTP module.
		 *  We issue a DENY first, and will re-run OnPreCommand later to trigger the other hooks post-DB update.
		 */
		ServerInstance->Modules.SetPriority(this, I_OnPreCommand, PRIORITY_FIRST);
	}
};

MODULE_INIT(ModuleSQLOper)
