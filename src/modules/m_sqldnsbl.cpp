#include "inspircd.h"
#include "xline.h"
#include "modules/dns.h"
#include "modules/sql.h"
#include "modules/regex.h"

struct SQLDNSBLList {
    std::string id;
    std::string name;
    std::string domain;
    std::string regex;
    std::string priority;
    std::string web;
    std::string active;
};

class SQLDNSBLResolver : public DNS::Request
{
	std::string theiruid;
	LocalStringExt& nameExt;
	LocalIntExt& countExt;
	SQLDNSBLList ConfEntry;
	dynamic_reference<RegexFactory> RegexEngine;
 public:
	SQLDNSBLResolver(DNS::Manager *mgr, Module *me, LocalStringExt& match, LocalIntExt& ctr, std::string hostname, LocalUser* u, SQLDNSBLList conf)
		: DNS::Request(mgr, me, hostname, DNS::QUERY_A, true), theiruid(u->uuid), nameExt(match), countExt(ctr), ConfEntry(conf), RegexEngine(me, "regex")
	{
	}

	void OnLookupComplete(const DNS::Query *r) CXX11_OVERRIDE
	{
		/* Check the user still exists */
		LocalUser* them = (LocalUser*)ServerInstance->FindUUID(theiruid);
		if (!them) {
	        ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "DNSBL: User %s is no longer here", them->nick.c_str());
			return;
        }

		const DNS::ResourceRecord* const ans_record = r->FindAnswerOfType(DNS::QUERY_A);
		if (!ans_record)
			return;

		// All replies should be in 127.0.0.0/8
		if (ans_record->rdata.compare(0, 4, "127.") != 0)
		{
	        ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "DNSBL: %s returned address outside of acceptable subnet 127.0.0.0/8: %s", ConfEntry.domain.c_str(), ans_record->rdata.c_str());
			return;
		}

        // Checking answer with PCRE Regex
        Regex* regex = RegexEngine->Create(ConfEntry.regex);
        if (!regex->Matches(ans_record->rdata)) {
	        ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "DNSBL: %s returned address not matching regex: %s %s", ConfEntry.domain.c_str(), ConfEntry.regex.c_str(), ans_record->rdata.c_str());
            return;
        }


        // Applying G-Line
        std::string reason = "Tu IP está en la lista negra "+ConfEntry.name+". Para solicitar la eliminación, visita "+ConfEntry.web+".";
		GLine* gl = new GLine(ServerInstance->Time(), 604800, ServerInstance->Config->ServerName.c_str(), reason.c_str(), "*", them->GetIPString());
		if (ServerInstance->XLines->AddLine(gl,NULL))
		{
			std::string timestr = InspIRCd::TimeString(gl->expiry);
			ServerInstance->SNO->WriteGlobalSno('x',"G:line added due to DNSBL match on *@%s to expire on %s: %s", them->GetIPString().c_str(), timestr.c_str(), reason.c_str());
	        ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "G:line added due to DNSBL match on *@%s to expire on %s: %s", them->GetIPString().c_str(), timestr.c_str(), reason.c_str());
			ServerInstance->XLines->ApplyLines();
		}
		else
		{
			delete gl;
			return;
		}
    }

	void OnError(const DNS::Query *q) CXX11_OVERRIDE
	{
	    ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "DNS fails with error %i for %s", q->error, q->question.name.c_str());
    }
};

class ModuleSQLDNSBL : public Module
{
private:
	dynamic_reference<DNS::Manager> DNS;
	dynamic_reference<SQL::Provider> SQL;
	LocalStringExt nameExt;
	LocalIntExt countExt;
    std::vector<SQLDNSBLList> List;
    int scans = 0;
    int UpdateAfter = 1;
protected:
    void UpdateLists();
    std::string GetReverseIP(LocalUser* user)
    {
        std::string reversedip;
	    if (user->client_sa.sa.sa_family == AF_INET)
	    {
		    unsigned int a, b, c, d;
    		d = (unsigned int) (user->client_sa.in4.sin_addr.s_addr >> 24) & 0xFF;
	    	c = (unsigned int) (user->client_sa.in4.sin_addr.s_addr >> 16) & 0xFF;
		    b = (unsigned int) (user->client_sa.in4.sin_addr.s_addr >> 8) & 0xFF;
		    a = (unsigned int) user->client_sa.in4.sin_addr.s_addr & 0xFF;

		    reversedip = ConvToStr(d) + "." + ConvToStr(c) + "." + ConvToStr(b) + "." + ConvToStr(a);
	    }
	    else if (user->client_sa.sa.sa_family == AF_INET6)
	    {
		    const unsigned char* ip = user->client_sa.in6.sin6_addr.s6_addr;

		    std::string buf = BinToHex(ip, 16);
		    for (std::string::const_reverse_iterator it = buf.rbegin(); it != buf.rend(); ++it)
		    {
			    reversedip.push_back(*it);
			    reversedip.push_back('.');
		    }
	    }
	    else
		    return "";

        return reversedip;
    }
public:
    ModuleSQLDNSBL()
    	: DNS(this, "DNS")
	    , SQL(this, "SQL")
		, nameExt("dnsbl_match", ExtensionItem::EXT_USER, this)
		, countExt("dnsbl_pending", ExtensionItem::EXT_USER, this)
    {
    }
    Version GetVersion() CXX11_OVERRIDE
    {
        return Version("Provides handling of DNS blacklists (config stored in SQL database)", VF_VENDOR);
    }
    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
    {
        UpdateLists();
    }
    void OnSetUserIP(LocalUser* user) CXX11_OVERRIDE
    {
        scans++;

        if (scans % UpdateAfter == UpdateAfter - 1) {
            UpdateLists();
        }

	    if ((user->exempt) || !DNS) {
		    return;
        }

        std::string reversedip = GetReverseIP(user);
        if (reversedip == "") {
            return;
        }

	    ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Reversed IP %s -> %s", user->GetIPString().c_str(), reversedip.c_str());

		for (unsigned i = 0; i < List.size(); ++i)
		{
            SQLDNSBLList item = List[i];
            std::string hostname = reversedip + "." + item.domain;

			SQLDNSBLResolver *r = new SQLDNSBLResolver(*this->DNS, this, nameExt, countExt, hostname, user, item);
			try
			{
				this->DNS->Process(r);
			}
			catch (DNS::Exception &ex)
			{
				delete r;
				ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, ex.GetReason());
			}

			if (user->quitting)
				break;
        }
    }
    void ClearList()
    {
        List.clear();
    }
    void AddList(std::string id, std::string name, std::string domain, std::string regex, std::string priority, std::string web, std::string active)
    {
	    ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Adding DNSBL %s %s %s %s %s %s %s", id.c_str(), name.c_str(), domain.c_str(), regex.c_str(), priority.c_str(), web.c_str(), active.c_str());
        SQLDNSBLList item;
        item.id = id;
        item.name = name;
        item.domain = domain;
        item.regex = regex;
        item.priority = priority;
        item.web = web;
        item.active = active;
        List.push_back(item);
    }
};

class SQLDNSBLQuery : public SQL::Query
{
private:
    ModuleSQLDNSBL* mod;
public:
	SQLDNSBLQuery(Module* me)
		: SQL::Query(me)
	{
        mod = (ModuleSQLDNSBL*)me;
	}

	void OnResult(SQL::Result& res) CXX11_OVERRIDE
	{
        mod->ClearList();
		SQL::Row row;
		while (res.GetRow(row))
		{
            mod->AddList(row[0], row[1], row[2], row[3], row[4], row[5], row[6]);
        }
    }

    void OnError(SQL::Error& error) CXX11_OVERRIDE
	{
		ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "query failed (%s)", error.ToString());
		ServerInstance->SNO->WriteGlobalSno('a', "m_sqldnsbl: failed fetch configuration from database");
	}
};

void ModuleSQLDNSBL::UpdateLists()
{
    ConfigTag* tag = ServerInstance->Config->ConfValue("sqldnsbl");
    std::string dbid = tag->getString("dbid");
    if (dbid.empty()) {
	    SQL.SetProvider("SQL");
	} else {
    	SQL.SetProvider("SQL/" + dbid);
    }
    std::string dbtable = tag->getString("dbtable");
    if (dbtable.empty()) {
        dbtable = "dnsbl";
    }
    std::string updateafter = tag->getString("updateafter");
    if (dbtable.empty()) {
        updateafter = "1";
    }
    UpdateAfter = std::atoi(updateafter.c_str());
    if (UpdateAfter < 1) {
        UpdateAfter = 1;
    }
    SQL->Submit(new SQLDNSBLQuery(this), "SELECT * FROM "+dbtable+" WHERE active=1 ORDER BY priority DESC");
}

MODULE_INIT(ModuleSQLDNSBL)
