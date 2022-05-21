/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Herman <GermanAizek@yandex.ru>
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2018 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2016 Johanna A
 *   Copyright (C) 2013-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2017, 2019-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2007 John Brooks <special@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2008, 2010 Craig Edwards <brain@inspircd.org>
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
#include "modules/isupport.h"
#include "modules/httpd.h"
#include "xline.h"

static ISupport::EventProvider* isevprov;

namespace Stats
{
	static const insp::flat_map<char, char const*>& xmlentities = {
		{ '<', "lt"   },
		{ '>', "gt"   },
		{ '&', "amp"  },
		{ '"', "quot" },
	};

	std::string Sanitize(const std::string& str)
	{
		std::string ret;
		ret.reserve(str.length() * 2);

		for (const auto& chr : str)
		{
			insp::flat_map<char, char const*>::const_iterator it = xmlentities.find(chr);
			if (it != xmlentities.end())
			{
				ret += '&';
				ret += it->second;
				ret += ';';
			}
			else if (chr == 0x09 || chr == 0x0A || chr == 0x0D || ((chr >= 0x20) && (chr <= 0x7e)))
			{
				// The XML specification defines the following characters as valid inside an XML document:
				// Char ::= #x9 | #xA | #xD | [#x20-#xD7FF] | [#xE000-#xFFFD] | [#x10000-#x10FFFF]
				ret += chr;
			}
			else
			{
				// If we reached this point then the string contains characters which can
				// not be represented in XML, even using a numeric escape. Therefore, we
				// Base64 encode the entire string and wrap it in a CDATA.
				ret.clear();
				ret += "<![CDATA[";
				ret += Base64::Encode(str);
				ret += "]]>";
				break;
			}
		}
		return ret;
	}

	void DumpMeta(std::ostream& data, Extensible* ext)
	{
		data << "<metadata>";
		for (const auto& [item, obj] : ext->GetExtList())
		{
			const std::string value = item->ToHuman(ext, obj);
			if (!value.empty())
				data << "<meta name=\"" << item->name << "\">" << Sanitize(value) << "</meta>";
			else if (!item->name.empty())
				data << "<meta name=\"" << item->name << "\"/>";
		}
		data << "</metadata>";
	}

	std::ostream& ServerInfo(std::ostream& data)
	{
		return data << "<server><name>" << ServerInstance->Config->ServerName << "</name><id>"
			<< ServerInstance->Config->GetSID() << "</id><description>"
			<< Sanitize(ServerInstance->Config->ServerDesc) << "</description><customversion>"
			<< Sanitize(ServerInstance->Config->CustomVersion) << "</customversion><version>"
			<< Sanitize(INSPIRCD_VERSION) << "</version></server>";
	}

	std::ostream& ISupport(std::ostream& data)
	{
		data << "<isupport>";

		ISupport::TokenMap tokens;
		isevprov->Call(&ISupport::EventListener::OnBuildISupport, tokens);
		for (const auto& [key, value] : tokens)
		{
			data << "<token><name>" << Sanitize(key)
				<< "</name><value>" << Sanitize(value)
				<< "</value></token>";
		}
		return data << "</isupport>";
	}

	std::ostream& General(std::ostream& data)
	{
		data << "<general>";
		data << "<usercount>" << ServerInstance->Users.GetUsers().size() << "</usercount>";
		data << "<localusercount>" << ServerInstance->Users.GetLocalUsers().size() << "</localusercount>";
		data << "<channelcount>" << ServerInstance->Channels.GetChans().size() << "</channelcount>";
		data << "<opercount>" << ServerInstance->Users.all_opers.size() << "</opercount>";
		data << "<socketcount>" << (SocketEngine::GetUsedFds()) << "</socketcount><socketmax>" << SocketEngine::GetMaxFds() << "</socketmax>";
		data << "<uptime><boot_time_t>" << ServerInstance->startup_time << "</boot_time_t></uptime>";
		data << "<currenttime>" << ServerInstance->Time() << "</currenttime>";

		data << ISupport;
		return data << "</general>";
	}

	std::ostream& XLines(std::ostream& data)
	{
		data << "<xlines>";
		for (const auto& xltype : ServerInstance->XLines->GetAllTypes())
		{
			XLineLookup* lookup = ServerInstance->XLines->GetAll(xltype);
			if (!lookup)
				continue;

			for (const auto& [_, xline] : *lookup)
			{
				data << "<xline type=\"" << xltype << "\"><mask>"
					<< Sanitize(xline->Displayable()) << "</mask><settime>"
					<< xline->set_time << "</settime><duration>" << xline->duration
					<< "</duration><reason>" << Sanitize(xline->reason)
					<< "</reason></xline>";
			}
		}
		return data << "</xlines>";
	}

	std::ostream& Modules(std::ostream& data)
	{
		data << "<modulelist>";
		for (const auto& [modname, mod] : ServerInstance->Modules.GetModules())
		{
			data << "<module><name>" << modname << "</name><description>"
				<< Sanitize(mod->description) << "</description></module>";
		}
		return data << "</modulelist>";
	}

	std::ostream& Channels(std::ostream& data)
	{
		data << "<channellist>";

		for (const auto& [_, c] : ServerInstance->Channels.GetChans())
		{
			data << "<channel>";
			data << "<usercount>" << c->GetUsers().size() << "</usercount><channelname>" << Sanitize(c->name) << "</channelname>";
			data << "<channeltopic>";
			data << "<topictext>" << Sanitize(c->topic) << "</topictext>";
			data << "<setby>" << Sanitize(c->setby) << "</setby>";
			data << "<settime>" << c->topicset << "</settime>";
			data << "</channeltopic>";
			data << "<channelmodes>" << Sanitize(c->ChanModes(true)) << "</channelmodes>";

			for (const auto& [__, memb] : c->GetUsers())
			{
				data << "<channelmember><uid>" << memb->user->uuid << "</uid><privs>"
					<< Sanitize(memb->GetAllPrefixChars()) << "</privs><modes>"
					<< memb->GetAllPrefixModes() << "</modes>";
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
			<< Sanitize(u->GetRealHost()) << "</realhost><displayhost>" << Sanitize(u->GetDisplayedHost()) << "</displayhost><realname>"
			<< Sanitize(u->GetRealName()) << "</realname><server>" << u->server->GetName() << "</server><signon>"
			<< u->signon << "</signon><age>" << u->age << "</age>";

		if (u->IsAway())
			data << "<away>" << Sanitize(u->awaymsg) << "</away><awaytime>" << u->awaytime << "</awaytime>";

		if (u->IsOper())
			data << "<opertype>" << Sanitize(u->oper->name) << "</opertype>";

		data << "<modes>" << u->GetModeLetters().substr(1) << "</modes><ident>" << Sanitize(u->ident) << "</ident>";

		LocalUser* lu = IS_LOCAL(u);
		if (lu)
			data << "<local/><port>" << lu->server_sa.port() << "</port><servaddr>"
				<< lu->server_sa.str() << "</servaddr><connectclass>"
				<< lu->GetClass()->GetName() << "</connectclass><lastmsg>"
				<< lu->idle_lastmsg << "</lastmsg>";

		data << "<ipaddress>" << Sanitize(u->GetIPString()) << "</ipaddress>";

		DumpMeta(data, u);

		data << "</user>";
		return data;
	}

	std::ostream& Users(std::ostream& data)
	{
		data << "<userlist>";
		for (const auto& [_, u] : ServerInstance->Users.GetUsers())
		{
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

		for (const auto& server : sl)
		{
			data << "<server>";
			data << "<servername>" << server.servername << "</servername>";
			data << "<parentname>" << server.parentname << "</parentname>";
			data << "<description>" << Sanitize(server.description) << "</description>";
			data << "<usercount>" << server.usercount << "</usercount>";
			data << "<opercount>" << server.opercount << "</opercount>";
			data << "<lagmillisecs>" << server.latencyms << "</lagmillisecs>";
			data << "</server>";
		}

		return data << "</serverlist>";
	}

	std::ostream& Commands(std::ostream& data)
	{
		data << "<commandlist>";

		for (const auto& [cmdname, cmd] : ServerInstance->Parser.GetCommands())
		{
			data << "<command><name>" << cmdname << "</name><usecount>" << cmd->use_count << "</usecount></command>";
		}
		return data << "</commandlist>";
	}

	enum OrderBy
	{
		OB_NICK,
		OB_LASTMSG,

		OB_NONE
	};

	struct UserSorter final
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
			switch (order)
			{
				case OB_LASTMSG:
					return Compare(IS_LOCAL(u1)->idle_lastmsg, IS_LOCAL(u2)->idle_lastmsg);
				case OB_NICK:
					return Compare(u1->nick, u2->nick);
				default:
					return false;
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
		for (const auto& [_, u] : ServerInstance->Users.GetUsers())
		{
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

class ModuleHttpStats final
	: public Module
	, public HTTPRequestEventListener
{
private:
	HTTPdAPI API;
	ISupport::EventProvider isupportevprov;
	bool enableparams = false;

public:
	ModuleHttpStats()
		: Module(VF_VENDOR, "Provides XML-serialised statistics about the server, channels, and users over HTTP via the /stats path.")
		, HTTPRequestEventListener(this)
		, API(this)
		, isupportevprov(this)
	{
		isevprov = &isupportevprov;
	}

	void ReadConfig(ConfigStatus& status) override
	{
		auto conf = ServerInstance->Config->ConfValue("httpstats");

		// Parameterized queries may cause a performance issue
		// Due to the sheer volume of data
		// So default them to disabled
		enableparams = conf->getBool("enableparams");
	}

	ModResult HandleRequest(HTTPRequest* http)
	{
		if (http->GetPath().compare(0, 6, "/stats"))
			return MOD_RES_PASSTHRU;

		ServerInstance->Logs.Debug(MODNAME, "Handling HTTP request for %s", http->GetPath().c_str());

		std::stringstream data;
		data << "<inspircdstats>";
		if (http->GetPath() == "/stats")
		{
			data << Stats::ServerInfo << Stats::General
				<< Stats::XLines << Stats::Modules
				<< Stats::Channels << Stats::Users
				<< Stats::Servers << Stats::Commands;
		}
		else if (http->GetPath() == "/stats/general")
		{
			data << Stats::General;
		}
		else if (http->GetPath() == "/stats/users")
		{
			if (enableparams)
				Stats::ListUsers(data, http->GetParsedURI().query_params);
			else
				data << Stats::Users;
		}
		else
		{
			return MOD_RES_PASSTHRU;
		}
		data << "</inspircdstats>";

		/* Send the document back to m_httpd */
		HTTPDocumentResponse response(this, *http, &data, 200);
		response.headers.SetHeader("X-Powered-By", MODNAME);
		response.headers.SetHeader("Content-Type", "text/xml");
		API->SendResponse(response);
		return MOD_RES_DENY; // Handled
	}

	ModResult OnHTTPRequest(HTTPRequest& req) override
	{
		return HandleRequest(&req);
	}
};

MODULE_INIT(ModuleHttpStats)
