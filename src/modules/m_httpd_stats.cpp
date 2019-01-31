/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2006-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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
#include "modules/httpd.h"
#include "xline.h"

namespace Stats
{
	struct Entities
	{
		static const insp::flat_map<char, char const*>& entities;
	};

	static const insp::flat_map<char, char const*>& init_entities()
	{
		static insp::flat_map<char, char const*> entities;
		entities['<'] = "lt";
		entities['>'] = "gt";
		entities['&'] = "amp";
		entities['"'] = "quot";
		return entities;
	}

	const insp::flat_map<char, char const*>& Entities::entities = init_entities();

	std::string Sanitize(const std::string& str)
	{
		std::string ret;
		ret.reserve(str.length() * 2);

		for (std::string::const_iterator x = str.begin(); x != str.end(); ++x)
		{
			insp::flat_map<char, char const*>::const_iterator it = Entities::entities.find(*x);

			if (it != Entities::entities.end())
			{
				ret += '&';
				ret += it->second;
				ret += ';';
			}
			else if (*x == 0x09 || *x == 0x0A || *x == 0x0D || ((*x >= 0x20) && (*x <= 0x7e)))
			{
				// The XML specification defines the following characters as valid inside an XML document:
				// Char ::= #x9 | #xA | #xD | [#x20-#xD7FF] | [#xE000-#xFFFD] | [#x10000-#x10FFFF]
				ret += *x;
			}
			else
			{
				// If we reached this point then the string contains characters which can
				// not be represented in XML, even using a numeric escape. Therefore, we
				// Base64 encode the entire string and wrap it in a CDATA.
				ret.clear();
				ret += "<![CDATA[";
				ret += BinToBase64(str);
				ret += "]]>";
				break;
			}
		}
		return ret;
	}

	void DumpMeta(std::ostream& data, Extensible* ext)
	{
		data << "<metadata>";
		for (Extensible::ExtensibleStore::const_iterator i = ext->GetExtList().begin(); i != ext->GetExtList().end(); i++)
		{
			ExtensionItem* item = i->first;
			std::string value = item->serialize(FORMAT_USER, ext, i->second);
			if (!value.empty())
				data << "<meta name=\"" << item->name << "\">" << Sanitize(value) << "</meta>";
			else if (!item->name.empty())
				data << "<meta name=\"" << item->name << "\"/>";
		}
		data << "</metadata>";
	}

	std::ostream& ServerInfo(std::ostream& data)
	{
		return data << "<server><name>" << ServerInstance->Config->ServerName << "</name><description>"
			<< Sanitize(ServerInstance->Config->ServerDesc) << "</description><version>"
			<< Sanitize(ServerInstance->GetVersionString(true)) << "</version></server>";
	}

	std::ostream& ISupport(std::ostream& data)
	{
		data << "<isupport>";
		const std::vector<Numeric::Numeric>& isupport = ServerInstance->ISupport.GetLines();
		for (std::vector<Numeric::Numeric>::const_iterator i = isupport.begin(); i != isupport.end(); ++i)
		{
			const Numeric::Numeric& num = *i;
			for (std::vector<std::string>::const_iterator j = num.GetParams().begin(); j != num.GetParams().end() - 1; ++j)
				data << "<token>" << Sanitize(*j) << "</token>";
		}
		return data << "</isupport>";
	}

	std::ostream& General(std::ostream& data)
	{
		data << "<general>";
		data << "<usercount>" << ServerInstance->Users->GetUsers().size() << "</usercount>";
		data << "<localusercount>" << ServerInstance->Users->GetLocalUsers().size() << "</localusercount>";
		data << "<channelcount>" << ServerInstance->GetChans().size() << "</channelcount>";
		data << "<opercount>" << ServerInstance->Users->all_opers.size() << "</opercount>";
		data << "<socketcount>" << (SocketEngine::GetUsedFds()) << "</socketcount><socketmax>" << SocketEngine::GetMaxFds() << "</socketmax>";
		data << "<uptime><boot_time_t>" << ServerInstance->startup_time << "</boot_time_t></uptime>";
		data << "<currenttime>" << ServerInstance->Time() << "</currenttime>";

		data << ISupport;
		return data << "</general>";
	}

	std::ostream& XLines(std::ostream& data)
	{
		data << "<xlines>";
		std::vector<std::string> xltypes = ServerInstance->XLines->GetAllTypes();
		for (std::vector<std::string>::iterator it = xltypes.begin(); it != xltypes.end(); ++it)
		{
			XLineLookup* lookup = ServerInstance->XLines->GetAll(*it);

			if (!lookup)
				continue;
			for (LookupIter i = lookup->begin(); i != lookup->end(); ++i)
			{
				data << "<xline type=\"" << it->c_str() << "\"><mask>"
					<< Sanitize(i->second->Displayable()) << "</mask><settime>"
					<< i->second->set_time << "</settime><duration>" << i->second->duration
					<< "</duration><reason>" << Sanitize(i->second->reason)
					<< "</reason></xline>";
			}
		}
		return data << "</xlines>";
	}

	std::ostream& Modules(std::ostream& data)
	{
		data << "<modulelist>";
		const ModuleManager::ModuleMap& mods = ServerInstance->Modules->GetModules();

		for (ModuleManager::ModuleMap::const_iterator i = mods.begin(); i != mods.end(); ++i)
		{
			Version v = i->second->GetVersion();
			data << "<module><name>" << i->first << "</name><description>" << Sanitize(v.description) << "</description></module>";
		}
		return data << "</modulelist>";
	}

	std::ostream& Channels(std::ostream& data)
	{
		data << "<channellist>";

		const chan_hash& chans = ServerInstance->GetChans();
		for (chan_hash::const_iterator i = chans.begin(); i != chans.end(); ++i)
		{
			Channel* c = i->second;

			data << "<channel>";
			data << "<usercount>" << c->GetUsers().size() << "</usercount><channelname>" << Sanitize(c->name) << "</channelname>";
			data << "<channeltopic>";
			data << "<topictext>" << Sanitize(c->topic) << "</topictext>";
			data << "<setby>" << Sanitize(c->setby) << "</setby>";
			data << "<settime>" << c->topicset << "</settime>";
			data << "</channeltopic>";
			data << "<channelmodes>" << Sanitize(c->ChanModes(true)) << "</channelmodes>";

			const Channel::MemberMap& ulist = c->GetUsers();
			for (Channel::MemberMap::const_iterator x = ulist.begin(); x != ulist.end(); ++x)
			{
				Membership* memb = x->second;
				data << "<channelmember><uid>" << memb->user->uuid << "</uid><privs>"
					<< Sanitize(memb->GetAllPrefixChars()) << "</privs><modes>"
					<< memb->modes << "</modes>";
				DumpMeta(data, memb);
				data << "</channelmember>";
			}

			DumpMeta(data, c);

			data << "</channel>";
		}

		return data << "</channellist>";
	}

	std::ostream& DumpUser(std::ostream& data, User* u)
	{
		data << "<user>";
		data << "<nickname>" << u->nick << "</nickname><uuid>" << u->uuid << "</uuid><realhost>"
			<< u->GetRealHost() << "</realhost><displayhost>" << u->GetDisplayedHost() << "</displayhost><realname>"
			<< Sanitize(u->GetRealName()) << "</realname><server>" << u->server->GetName() << "</server><signon>"
			<< u->signon << "</signon><age>" << u->age << "</age>";

		if (u->IsAway())
			data << "<away>" << Sanitize(u->awaymsg) << "</away><awaytime>" << u->awaytime << "</awaytime>";

		if (u->IsOper())
			data << "<opertype>" << Sanitize(u->oper->name) << "</opertype>";

		data << "<modes>" << u->GetModeLetters().substr(1) << "</modes><ident>" << Sanitize(u->ident) << "</ident>";

		LocalUser* lu = IS_LOCAL(u);
		if (lu)
			data << "<local/><port>" << lu->GetServerPort() << "</port><servaddr>"
				<< lu->server_sa.str() << "</servaddr><connectclass>"
				<< lu->GetClass()->GetName() << "</connectclass><lastmsg>"
				<< lu->idle_lastmsg << "</lastmsg>";

		data << "<ipaddress>" << u->GetIPString() << "</ipaddress>";

		DumpMeta(data, u);

		data << "</user>";
		return data;
	}

	std::ostream& Users(std::ostream& data)
	{
		data << "<userlist>";
		const user_hash& users = ServerInstance->Users->GetUsers();
		for (user_hash::const_iterator i = users.begin(); i != users.end(); ++i)
		{
			User* u = i->second;

			if (u->registered != REG_ALL)
				continue;

			DumpUser(data, u);
		}
		return data << "</userlist>";
	}

	std::ostream& Servers(std::ostream& data)
	{
		data << "<serverlist>";

		ProtocolInterface::ServerList sl;
		ServerInstance->PI->GetServerList(sl);

		for (ProtocolInterface::ServerList::const_iterator b = sl.begin(); b != sl.end(); ++b)
		{
			data << "<server>";
			data << "<servername>" << b->servername << "</servername>";
			data << "<parentname>" << b->parentname << "</parentname>";
			data << "<description>" << Sanitize(b->description) << "</description>";
			data << "<usercount>" << b->usercount << "</usercount>";
// This is currently not implemented, so, commented out.
//					data << "<opercount>" << b->opercount << "</opercount>";
			data << "<lagmillisecs>" << b->latencyms << "</lagmillisecs>";
			data << "</server>";
		}

		return data << "</serverlist>";
	}

	std::ostream& Commands(std::ostream& data)
	{
		data << "<commandlist>";

		const CommandParser::CommandMap& commands = ServerInstance->Parser.GetCommands();
		for (CommandParser::CommandMap::const_iterator i = commands.begin(); i != commands.end(); ++i)
		{
			data << "<command><name>" << i->second->name << "</name><usecount>" << i->second->use_count << "</usecount></command>";
		}
		return data << "</commandlist>";
	}

	enum OrderBy
	{
		OB_NICK,
		OB_LASTMSG,

		OB_NONE
	};

	struct UserSorter
	{
		OrderBy order;
		bool desc;

		UserSorter(OrderBy Order, bool Desc = false) : order(Order), desc(Desc) {}

		template <typename T>
		inline bool Compare(const T& a, const T& b)
		{
			return desc ? a > b : a < b;
		}

		bool operator()(User* u1, User* u2)
		{
			switch (order) {
				case OB_LASTMSG:
					return Compare(IS_LOCAL(u1)->idle_lastmsg, IS_LOCAL(u2)->idle_lastmsg);
					break;
				case OB_NICK:
					return Compare(u1->nick, u2->nick);
					break;
				default:
				case OB_NONE:
					return false;
					break;
			}
		}
	};

	std::ostream& ListUsers(std::ostream& data, const HTTPQueryParameters& params)
	{
		if (params.empty())
			return Users(data);

		data << "<userlist>";

		// Filters
		size_t limit = params.getNum<size_t>("limit");
		bool showunreg = params.getBool("showunreg");
		bool localonly = params.getBool("localonly");

		// Minimum time since a user's last message
		unsigned long min_idle = params.getDuration("minidle");
		time_t maxlastmsg = ServerInstance->Time() - min_idle;

		if (min_idle)
			// We can only check idle times on local users
			localonly = true;

		// Sorting
		const std::string& sortmethod = params.getString("sortby");
		bool desc = params.getBool("desc", false);

		OrderBy orderby;
		if (stdalgo::string::equalsci(sortmethod, "nick"))
			orderby = OB_NICK;
		else if (stdalgo::string::equalsci(sortmethod, "lastmsg"))
		{
			orderby = OB_LASTMSG;
			// We can only check idle times on local users
			localonly = true;
		}
		else
			orderby = OB_NONE;

		typedef std::list<User*> NewUserList;
		NewUserList user_list;
		user_hash users = ServerInstance->Users->GetUsers();
		for (user_hash::iterator i = users.begin(); i != users.end(); ++i)
		{
			User* u = i->second;
			if (!showunreg && u->registered != REG_ALL)
				continue;

			LocalUser* lu = IS_LOCAL(u);
			if (localonly && !lu)
				continue;

			if (min_idle && lu->idle_lastmsg > maxlastmsg)
				continue;

			user_list.push_back(u);
		}

		UserSorter sorter(orderby, desc);
		if (sorter.order != OB_NONE && !(!localonly && sorter.order == OB_LASTMSG))
			user_list.sort(sorter);

		size_t count = 0;
		for (NewUserList::const_iterator i = user_list.begin(); i != user_list.end() && (!limit || count < limit); ++i, ++count)
			DumpUser(data, *i);

		data << "</userlist>";
		return data;
	}
}

class ModuleHttpStats : public Module, public HTTPRequestEventListener
{
	HTTPdAPI API;
	bool enableparams;

 public:
	ModuleHttpStats()
		: HTTPRequestEventListener(this)
		, API(this)
		, enableparams(false)
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* conf = ServerInstance->Config->ConfValue("httpstats");

		// Parameterized queries may cause a performance issue
		// Due to the sheer volume of data
		// So default them to disabled
		enableparams = conf->getBool("enableparams");
	}

	ModResult HandleRequest(HTTPRequest* http)
	{
		std::string path = http->GetPath();

		if (path != "/stats" && path.substr(0, 7) != "/stats/")
			return MOD_RES_PASSTHRU;

		if (path[path.size() - 1] == '/')
			path.erase(path.size() - 1, 1);

		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Handling httpd event");

		bool found = true;
		std::stringstream data;
		data << "<inspircdstats>";

		if (path == "/stats")
		{
			data << Stats::ServerInfo << Stats::General
				<< Stats::XLines << Stats::Modules
				<< Stats::Channels << Stats::Users
				<< Stats::Servers << Stats::Commands;
		}
		else if (path == "/stats/general")
		{
			data << Stats::General;
		}
		else if (path == "/stats/users")
		{
			if (enableparams)
				Stats::ListUsers(data, http->GetParsedURI().query_params);
			else
				data << Stats::Users;
		}
		else
		{
			found = false;
		}

		if (found)
		{
			data << "</inspircdstats>";
		}
		else
		{
			data.clear();
			data.str(std::string());
		}

		/* Send the document back to m_httpd */
		HTTPDocumentResponse response(this, *http, &data, found ? 200 : 404);
		response.headers.SetHeader("X-Powered-By", MODNAME);
		response.headers.SetHeader("Content-Type", "text/xml");
		API->SendResponse(response);
		return MOD_RES_DENY; // Handled
	}

	ModResult OnHTTPRequest(HTTPRequest& req) CXX11_OVERRIDE
	{
		return HandleRequest(&req);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides statistics over HTTP via m_httpd", VF_VENDOR);
	}
};

MODULE_INIT(ModuleHttpStats)
