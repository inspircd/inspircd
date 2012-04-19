/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
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
#include "command_parse.h"
#include "sql.h"

class UserQuery : public SQLQuery
{
 public:
	const std::string uid;
	reference<ConfigTag> tag;
	UserQuery(Module* me, const std::string& u, ConfigTag* q)
		: SQLQuery(me), uid(u), tag(q)
	{
	}

	void OnResult(SQLResult& res)
	{
		User* user = ServerInstance->FindUUID(uid);
		if (!user)
			return;
		int rows = res.Rows();
		std::string format = tag->getString("rowformat");
		std::map<std::string, std::string> items;
		user->PopulateInfoMap(items);
		if (!format.empty())
		{
			SQLEntries result;
			std::vector<std::string> cols;
			res.GetCols(cols);
			while (res.GetRow(result))
			{
				std::map<std::string, std::string> row(items);
				for(unsigned int i=0; i < cols.size(); i++)
				{
					if (result[i].nul)
						continue;
					row.insert(std::make_pair(cols[i], result[i].value));
				}

				user->SendText(MapFormatSubst(format, row));
			}
		}
		format = tag->getString("resultformat");
		if (!format.empty())
		{
			items.insert(std::make_pair("rows", ConvToStr(rows)));
			user->SendText(MapFormatSubst(format, items));
		}
	}

	void OnError(SQLerror& error)
	{
		User* user = ServerInstance->FindUUID(uid);
		if (!user)
			return;
		std::string format = tag->getString("errorformat", ":$server NOTICE $nick :SQL command failed: $msg");
		if (!format.empty())
		{
			std::map<std::string, std::string> items;
			user->PopulateInfoMap(items);
			items.insert(std::make_pair("msg", error.str));
			user->SendText(MapFormatSubst(format, items));
		}
	}
};

class SQLCommand : public Command
{
 public:
	reference<ConfigTag> tag;
	dynamic_reference<SQLProvider> db;
	SQLCommand(Module* Parent, ConfigTag* Tag)
		: Command(Parent, Tag->getString("name")), tag(Tag), db("SQL/" + Tag->getString("dbid"))
	{
		syntax = Tag->getString("syntax");
		if (Tag->getBool("operonly", true))
			flags_needed = 'o';
		if (!db)
			throw CoreException("Could not find database");
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
    {
		if (!db)
			return CMD_FAILURE;

		ParamM userinfo;
		user->PopulateInfoMap(userinfo);
		for(unsigned int i=0; i < parameters.size(); i++)
			userinfo.insert(std::make_pair(ConvToStr(i + 1), parameters[i]));
		std::string fmt = tag->getString("queryformat");

		db->submit(new UserQuery(creator, user->uuid, tag), fmt, userinfo);
		return CMD_SUCCESS;
	}
};

class ModuleSQLCommand : public Module
{
 public:
	std::vector<SQLCommand*> cmds;
	void init()
	{
		// everything is in readconfig
	}

	void ReadConfig(ConfigReadStatus& status)
	{
		for(std::vector<SQLCommand*>::iterator i = cmds.begin(); i != cmds.end(); i++)
		{
			ServerInstance->Parser->RemoveCommand(*i);
			delete *i;
		}
		cmds.clear();
		ConfigTagList tags = ServerInstance->Config->GetTags("sqlcommand");
		for(ConfigIter i = tags.first; i != tags.second; i++)
		{
			ConfigTag* tag = i->second;
			SQLCommand* cmd = NULL;
			try
			{
				cmd = new SQLCommand(this, tag);
				ServerInstance->Modules->AddService(*cmd);
				cmds.push_back(cmd);
			}
			catch (CoreException& e)
			{
				status.ReportError(tag, e.err.c_str(), false);
				delete cmd;
			}
		}
	}

	~ModuleSQLCommand()
	{
		for(std::vector<SQLCommand*>::iterator i = cmds.begin(); i != cmds.end(); i++)
			delete *i;
	}

	Version GetVersion()
	{
		return Version("Add commands that execute arbitrary SQL queries", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSQLCommand)
