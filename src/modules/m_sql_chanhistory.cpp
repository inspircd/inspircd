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

/* $ModDesc: Provides channel history for a given number of lines, stored in an SQL database */

namespace m_sql_chanhistory
{

class DiscardQuery : public SQLQuery
{
 public:
	DiscardQuery(Module* me) : SQLQuery(me) {}
	void OnResult(SQLResult& res) {}
	void OnError(SQLerror& e)
	{
		ServerInstance->Logs->Log("m_sql_chanhistory", DEFAULT, "SQL update returned error: %s", e.str.c_str());
	}
};

class ReplayQuery : public SQLQuery
{
 public:
	std::string cname, uid;
	ReplayQuery(Module* me, Membership* memb)
		: SQLQuery(me), cname(memb->chan->name), uid(memb->user->uuid)
	{
	}

	void OnResult(SQLResult& res)
	{
		User* user = ServerInstance->FindUUID(uid);
		Channel* chan = ServerInstance->FindChan(cname);
		int rows = res.Rows();
		if (!user || !chan || !rows)
			return;

		user->WriteServ("NOTICE %s :Replaying %d lines of pre-join history", chan->name.c_str(), rows);
		SQLEntries row;
		while (res.GetRow(row))
		{
			user->Write(row[0]);
		}
	}

	void OnError(SQLerror& e)
	{
		ServerInstance->Logs->Log("m_sql_chanhistory", DEFAULT, "SQL query returned error: %s", e.str.c_str());
	}
};

class HistoryMode : public ModeHandler
{
 public:
	LocalIntExt histID;
	int maxlines;
	dynamic_reference<SQLProvider> sqldb;
	std::string tablename;

	HistoryMode(Module* Creator) : ModeHandler(Creator, "history", 'H', PARAM_SETONLY, MODETYPE_CHANNEL),
		histID(EXTENSIBLE_CHANNEL, "history-id", Creator), sqldb("SQL") { fixed_letter = false; }

	std::pair<int, int> ParamParse(const std::string& parameter)
	{
		std::string::size_type colon = parameter.find(':');
		if (colon == std::string::npos)
			return std::make_pair(0,0);
		int len = atoi(parameter.substr(0, colon).c_str());
		int time = ServerInstance->Duration(parameter.substr(colon+1));
		return std::make_pair(len,time);
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			std::pair<int, int> rv = ParamParse(parameter);
			int len = rv.first;
			int time = rv.second;
			if (len <= 0 || time < 0)
				return MODEACTION_DENY;
			if (len > maxlines && IS_LOCAL(source))
				return MODEACTION_DENY;
			if (parameter == channel->GetModeParameter(this))
				return MODEACTION_DENY;
			channel->SetModeParam(this, parameter);
		}
		else
		{
			channel->SetModeParam(this, "");
		}

		ParamL n;
		n.push_back(tablename);
		n.push_back(channel->name);
		sqldb->submit(new DiscardQuery(creator), "DELETE FROM ? WHERE chan = '?'", n);

		histID.set(channel, 0);
		return MODEACTION_ALLOW;
	}
};

class ModuleChanHistory : public Module
{
	HistoryMode m;
 public:
	ModuleChanHistory() : m(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(m);
		ServerInstance->Modules->AddService(m.histID);

		Implementation eventlist[] = { I_OnPostJoin, I_OnUserMessage, I_OnChannelDelete };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));

		ParamL n;
		n.push_back(m.tablename);
		m.sqldb->submit(new DiscardQuery(this), "DELETE FROM ?", n);
	}

	void ReadConfig(ConfigReadStatus& status)
	{
		ConfigTag *tag = ServerInstance->Config->GetTag("chanhistory");
		m.maxlines = tag->getInt("maxlines", 50);
		m.tablename = tag->getString("table", "chanhistory");
		if (m.tablename.empty())
			status.ReportError(tag, "Table name cannot be empty");
		std::string dbid = tag->getString("dbid");
		if (dbid.empty())
			m.sqldb.SetProvider("SQL");
		else
			m.sqldb.SetProvider("SQL/" + dbid);
		if (!m.sqldb)
			status.ReportError(tag, "SQL database not found!");
	}

	~ModuleChanHistory()
	{
		ServerInstance->Modes->DelMode(&m);
	}

	void OnUserMessage(User* user,void* dest,int target_type, const std::string &text, char status, const CUList&)
	{
		if (target_type == TYPE_CHANNEL && status == 0)
		{
			Channel* c = (Channel*)dest;
			if (!c->IsModeSet(&m))
				return;

			char buf[MAXBUF];
			snprintf(buf, MAXBUF, ":%s PRIVMSG %s :%s", user->GetFullHost().c_str(), c->name.c_str(), text.c_str());

			int seq = m.histID.get(c);
			m.histID.set(c, seq + 1);

			ParamL n;
			n.push_back(m.tablename);
			n.push_back(c->name);
			n.push_back(ConvToStr(seq));
			n.push_back(ConvToStr(ServerInstance->Time()));
			n.push_back(buf);
			m.sqldb->submit(new DiscardQuery(this),
				"INSERT INTO ? (chan, line, ts, text) VALUES ('?', '?', '?', '?')", n);
			n.clear();

			std::pair<int, int> param = m.ParamParse(c->GetModeParameter(&m));
			n.push_back(m.tablename);
			n.push_back(c->name);
			if (param.first < seq)
				n.push_back(ConvToStr(seq - param.first));
			else
				n.push_back("0");
			n.push_back(ConvToStr(ServerInstance->Time() - param.second));
			m.sqldb->submit(new DiscardQuery(this),
				"DELETE FROM ? WHERE chan = '?' AND (line < ? OR ts < ?)", n);
		}
	}

	void OnPostJoin(Membership* memb)
	{
		if (!memb->chan->IsModeSet(&m))
			return;
		std::pair<int, int> param = m.ParamParse(memb->chan->GetModeParameter(&m));
		int seq = m.histID.get(memb->chan);
		ParamL n;
		n.push_back(m.tablename);
		n.push_back(memb->chan->name);
		if (param.first < seq)
			n.push_back(ConvToStr(seq - param.first));
		else
			n.push_back("0");
		n.push_back(ConvToStr(ServerInstance->Time() - param.second));

		m.sqldb->submit(new ReplayQuery(this, memb),
			"SELECT text FROM ? WHERE chan = '?' AND line >= ? AND ts >= ? ORDER BY line", n);
	}

	void OnChannelDelete(Channel* chan)
	{
		if (chan->IsModeSet(&m))
		{
			ParamL n;
			n.push_back(m.tablename);
			n.push_back(chan->name);
			m.sqldb->submit(new DiscardQuery(this), "DELETE FROM ? WHERE chan = '?'", n);
		}
	}

	Version GetVersion()
	{
		return Version("Provides channel history replayed on join", VF_VENDOR);
	}
};

}

using m_sql_chanhistory::ModuleChanHistory;

MODULE_INIT(ModuleChanHistory)
