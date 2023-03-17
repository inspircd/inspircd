/*
 *
 * (C) 2013-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

#include "irc2sql.h"

void IRC2SQL::RunQuery(const SQL::Query &q) {
    if (sql) {
        sql->Run(&sqlinterface, q);
    }
}

void IRC2SQL::GetTables() {
    TableList.clear();
    ProcedureList.clear();
    EventList.clear();
    if (!sql) {
        return;
    }

    SQL::Result r = this->sql->RunQuery(this->sql->GetTables(prefix));
    for (int i = 0; i < r.Rows(); ++i) {
        const std::map<Anope::string, Anope::string> &map = r.Row(i);
        for (std::map<Anope::string, Anope::string>::const_iterator it = map.begin();
                it != map.end(); ++it) {
            TableList.push_back(it->second);
        }
    }
    query = "SHOW PROCEDURE STATUS WHERE `Db` = Database();";
    r = this->sql->RunQuery(query);
    for (int i = 0; i < r.Rows(); ++i) {
        ProcedureList.push_back(r.Get(i, "Name"));
    }
    query = "SHOW EVENTS WHERE `Db` = Database();";
    r = this->sql->RunQuery(query);
    for (int i = 0; i < r.Rows(); ++i) {
        EventList.push_back(r.Get(i, "Name"));
    }
}

bool IRC2SQL::HasTable(const Anope::string &table) {
    for (std::vector<Anope::string>::const_iterator it = TableList.begin();
            it != TableList.end(); ++it)
        if (*it == table) {
            return true;
        }
    return false;
}

bool IRC2SQL::HasProcedure(const Anope::string &table) {
    for (std::vector<Anope::string>::const_iterator it = ProcedureList.begin();
            it != ProcedureList.end(); ++it)
        if (*it == table) {
            return true;
        }
    return false;
}

bool IRC2SQL::HasEvent(const Anope::string &table) {
    for (std::vector<Anope::string>::const_iterator it = EventList.begin();
            it != EventList.end(); ++it)
        if (*it == table) {
            return true;
        }
    return false;
}
