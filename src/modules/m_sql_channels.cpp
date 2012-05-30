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
#include "sql.h"

/* $ModDesc: Provides channel state updates to an SQL database */

namespace m_sql_channels
{

static bool reading = false;

class DatabaseReadQuery : public SQLQuery
{
 public:
	LocalIntExt& InDB;
	DatabaseReadQuery(Module* me, LocalIntExt& indb) : SQLQuery(me), InDB(indb)
	{
	}

	void OnResult(SQLResult& res)
	{
		SQLEntries row;
		reading = true;
		while (res.GetRow(row))
		{
			std::string channame = row[0];
			Channel* chan = ServerInstance->FindChan(channame);
			if (!chan)
			{
				time_t ts = atol(row[1].value.c_str());
				if (!ts)
					ts = ServerInstance->Time();
				chan = new Channel(channame, ts);
			}
			InDB.set(chan, 1);
			if (!row[2].nul)
			{
				irc::spacesepstream modes(row[2]);
				std::string mode;
				irc::modestacker ms;
				while (modes.GetToken(mode))
				{
					std::string::size_type eq = mode.find('=');
					std::string name = mode.substr(0, eq);
					std::string value;
					if (eq != std::string::npos)
						value = mode.substr(eq + 1);
					ModeHandler *mh = ServerInstance->Modes->FindMode(name);
					if (!mh)
						continue;
					ms.push(irc::modechange(mh->id, value, true));
				}
				ServerInstance->Modes->Process(ServerInstance->FakeClient, chan, ms);
			}
			if (!row[3].nul)
			{
				chan->topic = row[3];
				chan->setby = row[4];
				chan->topicset = atol(row[5].value.c_str());
			}
			// delete the channel if it is empty and not +r or +P
			chan->DelUser(ServerInstance->FakeClient);
		}
		reading = false;
	}
	void OnError(SQLerror& e)
	{
		ServerInstance->Logs->Log("m_sql_channels", SPARSE, "m_sql_channels got error on initial query: %s", e.str.c_str());
	}
};

class DiscardQuery : public SQLQuery
{
 public:
	DiscardQuery(Module* me) : SQLQuery(me) {}
	void OnResult(SQLResult& res) {}
	void OnError(SQLerror& e)
	{
		ServerInstance->Logs->Log("m_sql_channels", DEFAULT, "SQL update returned error: %s", e.str.c_str());
	}
};

class ChanSQLDB : public Module
{
 private:
	std::string tablename;
	dynamic_reference<SQLProvider> sqldb;
	LocalIntExt InDB;

 public:
	ChanSQLDB() : sqldb("SQL"), InDB(EXTENSIBLE_CHANNEL, "in_sql_chans", this)
	{
	}
	Version GetVersion()
	{
		return Version("Provides channel state updates to an SQL database", VF_VENDOR);
	}
	void init()
	{
		Implementation eventlist[] = {
			I_OnMode, I_OnPostTopicChange, I_OnUserJoin, I_OnChannelDelete
		};

		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		ServerInstance->Modules->AddService(InDB);

		ParamL n;
		n.push_back(tablename);
		// just dump the entire thing back to us, please
		sqldb->submit(new DatabaseReadQuery(this, InDB),
			"SELECT name, ts, modes, topic, topicset, topicts FROM ?", n);
	}
	void ReadConfig(ConfigReadStatus& status)
	{
		ConfigTag *tag = ServerInstance->Config->GetTag ("chandb");

		tablename = tag->getString("table", "channels");
		if (tablename.empty())
			status.ReportError(tag, "Table name cannot be empty");
		std::string dbid = tag->getString("dbid");
		if (dbid.empty())
			sqldb.SetProvider("SQL");
		else
			sqldb.SetProvider("SQL/" + dbid);
		if (!sqldb)
			status.ReportError(tag, "SQL database not found!");
	}

	void AddToDB(Channel* chan)
	{
		irc::modestacker ms;
		chan->ChanModes(ms, MODELIST_FULL);
		ParamL n;
		n.push_back(tablename);
		n.push_back(chan->name);
		n.push_back(ConvToStr(chan->age));
		n.push_back(ms.popModeLine(FORMAT_PERSIST, INT_MAX, INT_MAX));
		if (chan->topic.empty())
		{
			sqldb->submit(new DiscardQuery(this),
				"INSERT INTO ? (name, ts, modes) VALUES ('?', '?', '?')", n);
		}
		else
		{
			n.push_back(chan->topic);
			n.push_back(chan->setby);
			n.push_back(ConvToStr(chan->topicset));
			sqldb->submit(new DiscardQuery(this),
				"INSERT INTO ? (name, ts, modes, topic, topicset, topicts) VALUES ('?', '?', '?', '?', '?', '?')", n);
		}
		InDB.set(chan, 1);
	}
	void OnUserJoin(Membership* memb, bool, bool, CUList&)
	{
		if (!InDB.get(memb->chan))
			AddToDB(memb->chan);
	}
	void OnPostTopicChange (User *user, Channel *chan, const std::string &topic)
	{
		if (reading || !InDB.get(chan))
			return;
		ParamL n;
		n.push_back(tablename);
		n.push_back(chan->topic);
		n.push_back(chan->setby);
		n.push_back(ConvToStr(chan->topicset));
		n.push_back(chan->name);
		sqldb->submit(new DiscardQuery(this),
			"UPDATE ? SET (topic, topicset, topicts) = ('?', '?', '?') WHERE name = '?'", n);
	}

	void OnMode (User *user, Extensible *target, const irc::modestacker &modes)
	{
		Channel *chan = IS_CHANNEL(target);
		if (reading || !chan || modes.empty())
			return;
		if (!InDB.get(chan))
		{
			AddToDB(chan);
			return;
		}
		irc::modestacker ms;
		chan->ChanModes(ms, MODELIST_FULL);
		ParamL n;
		n.push_back(tablename);
		n.push_back(ms.popModeLine(FORMAT_PERSIST, INT_MAX, INT_MAX));
		n.push_back(chan->name);
		sqldb->submit(new DiscardQuery(this),
			"UPDATE ? SET modes = '?' WHERE name = '?'", n);
	}

	void OnChannelDelete(Channel* chan)
	{
		if (!InDB.get(chan))
			return;
		ParamL n;
		n.push_back(tablename);
		n.push_back(chan->name);
		sqldb->submit(new DiscardQuery(this),
			"DELETE FROM ? WHERE name = '?'", n);
	}

	void Prioritize()
	{
		// database reading may depend on channel modes being loaded
		ServerInstance->Modules->SetPriority(this, I_ModuleInit, PRIORITY_LAST);
	}
};
}

using m_sql_channels::ChanSQLDB;

MODULE_INIT(ChanSQLDB)
