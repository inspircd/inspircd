/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2016 Johanna A
 *   Copyright (C) 2013, 2020-2023, 2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013, 2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2007 John Brooks <john@jbrooks.io>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2008 Craig Edwards <brain@inspircd.org>
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
#include "extension.h"
#include "modules/isupport.h"
#include "modules/httpd.h"
#include "xline.h"

#include <stack>

static ISupport::EventProvider* isevprov;

namespace Stats
{
	static const insp::flat_map<char, char const*> xmlentities = {
		{ '<', "lt"   },
		{ '>', "gt"   },
		{ '&', "amp"  },
		{ '"', "quot" },
	};

	std::string Sanitize(const std::string& str)
	{
		std::string ret;
		ret.reserve(str.length() * 2);

		for (const auto chr : str)
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
				ret += Base64::Encode(str, nullptr, '=');
				ret += "]]>";
				break;
			}
		}
		return ret;
	}

	class XMLSerializer final
	{
	private:
		std::stack<const char*> blocks;
		std::stringstream data;

	public:
		XMLSerializer& Attribute(const char* name, const std::string& value)
		{
			if (value.empty())
				data << '<' << name << "/>";
			else
				data << '<' << name << '>' << Sanitize(value) << "</" << name << '>';
			return *this;
		}

		template<typename Numeric>
		std::enable_if_t<std::is_arithmetic_v<Numeric>, XMLSerializer&> Attribute(const char* name, const Numeric& value)
		{
			return Attribute(name, ConvToStr(value));
		}

		std::stringstream* GetData() { return &data; }

		XMLSerializer& BeginBlock(const char* name)
		{
			blocks.push(name);
			data << '<' << name << '>';
			return *this;
		}

		XMLSerializer& EndBlock()
		{
			const char* name = blocks.top();
			data << "</" << name << '>';
			blocks.pop();
			return *this;
		}
	};

	void DumpMeta(XMLSerializer& serializer, Extensible* ext)
	{
		serializer.BeginBlock("metadata");
		for (const auto& [item, obj] : ext->GetExtList())
		{
			serializer.BeginBlock("meta")
				.Attribute("name", item->name);

			const std::string value = item->ToHuman(ext, obj);
			serializer.Attribute("value", value)
				.EndBlock();
		}
		serializer.EndBlock();
	}

	void ServerInfo(XMLSerializer& serializer)
	{
		serializer.BeginBlock("server")
			.Attribute("id", ServerInstance->Config->ServerId)
			.Attribute("name", ServerInstance->Config->ServerName)
			.Attribute("description", ServerInstance->Config->ServerDesc)
			.Attribute("customversion", ServerInstance->Config->CustomVersion)
			.Attribute("version", INSPIRCD_VERSION)
			.EndBlock();
	}

	void ISupport(XMLSerializer& serializer)
	{
		ISupport::TokenMap tokens;
		isevprov->Call(&ISupport::EventListener::OnBuildISupport, tokens);

		serializer.BeginBlock("isupport");
		for (const auto& [key, value] : tokens)
		{
			serializer.BeginBlock("token")
				.Attribute("name", key)
				.Attribute("value", value)
				.EndBlock();
		}
		serializer.EndBlock();
	}

	void General(XMLSerializer& serializer)
	{
		serializer.BeginBlock("general")
			.Attribute("usercount", ServerInstance->Users.GetUsers().size())
			.Attribute("localusercount", ServerInstance->Users.GetLocalUsers().size())
			.Attribute("channelcount", ServerInstance->Channels.GetChans().size())
			.Attribute("opercount", ServerInstance->Users.all_opers.size())
			.Attribute("socketcount", SocketEngine::GetUsedFds())
			.Attribute("socketmax", SocketEngine::GetMaxFds())
			.Attribute("boottime", ServerInstance->StartTime)
			.Attribute("currenttime", ServerInstance->Time());

		ISupport(serializer);
		serializer.EndBlock();
	}

	void XLines(XMLSerializer& serializer)
	{
		serializer.BeginBlock("xlines");
		for (const auto& xltype : ServerInstance->XLines->GetAllTypes())
		{
			XLineLookup* lookup = ServerInstance->XLines->GetAll(xltype);
			if (!lookup)
				continue;

			for (const auto& [_, xline] : *lookup)
			{
				serializer.BeginBlock("xline")
					.Attribute("type", xltype)
					.Attribute("mask", xline->Displayable())
					.Attribute("settime", xline->set_time)
					.Attribute("duration", xline->duration)
					.Attribute("reason", xline->reason)
					.EndBlock();
			}
		}
		serializer.EndBlock();
	}

	void Modules(XMLSerializer& serializer)
	{
		serializer.BeginBlock("modulelist");
		for (const auto& [modname, mod] : ServerInstance->Modules.GetModules())
		{
			serializer.BeginBlock("module")
				.Attribute("name", modname)
				.Attribute("description", mod->description)
				.EndBlock();
		}
		serializer.EndBlock();
	}

	void Channels(XMLSerializer& serializer)
	{
		serializer.BeginBlock("channellist");
		for (const auto& [_, c] : ServerInstance->Channels.GetChans())
		{
			serializer.BeginBlock("channel")
				.Attribute("channelname", c->name)
				.Attribute("usercount", c->GetUsers().size())
				.Attribute("channelmodes", c->ChanModes(true));

			if (!c->topic.empty())
			{
				serializer.BeginBlock("channeltopic")
					.Attribute("topictext", c->topic)
					.Attribute("setby", c->setby)
					.Attribute("settime", c->topicset)
					.EndBlock();
			}

			for (const auto& [__, memb] : c->GetUsers())
			{
				serializer.BeginBlock("channelmember")
					.Attribute("uid", memb->user->uuid)
					.Attribute("privs", memb->GetAllPrefixChars())
					.Attribute("modes", memb->GetAllPrefixModes());

				DumpMeta(serializer, memb);
				serializer.EndBlock();
			}

			DumpMeta(serializer, c);
			serializer.EndBlock();
		}
		serializer.EndBlock();
	}

	void DumpUser(XMLSerializer& serializer, User* u)
	{
		serializer.BeginBlock("user")
			.Attribute("nickname", u->nick)
			.Attribute("uuid", u->uuid)
			.Attribute("realhost", u->GetRealHost())
			.Attribute("displayhost", u->GetDisplayedHost())
			.Attribute("realname", u->GetRealName())
			.Attribute("server", u->server->GetName())
			.Attribute("signon", u->signon)
			.Attribute("nickchanged", u->nickchanged)
			.Attribute("modes", u->GetModeLetters().substr(1))
			.Attribute("realuser", u->GetRealUser())
			.Attribute("displayuser", u->GetDisplayedUser())
			.Attribute("ipaddress", u->GetAddress());

		if (u->IsAway())
		{
			serializer.Attribute("away", u->away->message)
				.Attribute("awaytime", u->away->time);
		}

		if (u->IsOper())
			serializer.Attribute("opertype", u->oper->GetType());

		LocalUser* lu = IS_LOCAL(u);
		if (lu)
		{
			serializer.Attribute("port", lu->server_sa.port())
				.Attribute("servaddr", lu->server_sa.addr())
				.Attribute("connectclass", lu->GetClass()->GetName())
				.Attribute("lastmsg", lu->idle_lastmsg);
		}

		DumpMeta(serializer, u);
		serializer.EndBlock();
	}

	void Users(XMLSerializer& serializer)
	{
		serializer.BeginBlock("userlist");
		for (const auto& [_, u] : ServerInstance->Users.GetUsers())
		{
			if (!u->IsFullyConnected())
				continue;

			DumpUser(serializer, u);
		}
		serializer.EndBlock();
	}

	void Servers(XMLSerializer& serializer)
	{
		ProtocolInterface::ServerList sl;
		ServerInstance->PI->GetServerList(sl);

		serializer.BeginBlock("serverlist");
		for (const auto& server : sl)
		{
			serializer.BeginBlock("server")
				.Attribute("servername", server.servername)
				.Attribute("parentname", server.parentname)
				.Attribute("description", server.description)
				.Attribute("usercount", server.usercount)
				.Attribute("opercount", server.opercount)
				.Attribute("lagmillisecs", server.latencyms)
				.EndBlock();
		}
		serializer.EndBlock();
	}

	void Commands(XMLSerializer& serializer)
	{
		serializer.BeginBlock("commandlist");
		for (const auto& [cmdname, cmd] : ServerInstance->Parser.GetCommands())
		{
			serializer.BeginBlock("command")
				.Attribute("name", cmdname)
				.Attribute("usecount", cmd->use_count)
				.EndBlock();
		}
		serializer.EndBlock();
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

		UserSorter(OrderBy Order, bool Desc = false)
			: order(Order)
			, desc(Desc)
		{
		}

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

	void ListUsers(XMLSerializer& serializer, const HTTPQueryParameters& params)
	{
		if (params.empty())
		{
			Users(serializer);
			return;
		}

		serializer.BeginBlock("userlist");

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
		if (insp::equalsci(sortmethod, "nick"))
			orderby = OB_NICK;
		else if (insp::equalsci(sortmethod, "lastmsg"))
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
			if (!showunreg && !u->IsFullyConnected())
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
			DumpUser(serializer, *i);

		serializer.EndBlock();
	}
}

class ModuleHttpStats final
	: public Module
	, public HTTPRequestEventListener
{
private:
	HTTPdAPI API;
	ISupport::EventProvider isupportevprov;

public:
	ModuleHttpStats()
		: Module(VF_VENDOR, "Provides XML-serialised statistics about the server, channels, and users over HTTP via the /stats path.")
		, HTTPRequestEventListener(this)
		, API(this)
		, isupportevprov(this)
	{
		isevprov = &isupportevprov;
	}

	ModResult OnHTTPRequest(HTTPRequest& request) override
	{
		if (request.GetPath().compare(0, 6, "/stats"))
			return MOD_RES_PASSTHRU;

		ServerInstance->Logs.Debug(MODNAME, "Handling HTTP request for {}", request.GetPath());

		Stats::XMLSerializer serializer;
		serializer.BeginBlock("inspircdstats");
		if (request.GetPath() == "/stats")
		{
			Stats::ServerInfo(serializer);
			Stats::General(serializer);
			Stats::XLines(serializer);
			Stats::Modules(serializer);
			Stats::Channels(serializer);
			Stats::Users(serializer);
			Stats::Servers(serializer);
			Stats::Commands(serializer);
		}
		else if (request.GetPath() == "/stats/general")
		{
			Stats::General(serializer);
		}
		else if (request.GetPath() == "/stats/users")
		{
			Stats::ListUsers(serializer, request.GetParsedURI().query_params);
		}
		else
		{
			return MOD_RES_PASSTHRU;
		}
		serializer.EndBlock();

		/* Send the document back to m_httpd */
		HTTPDocumentResponse response(this, request, serializer.GetData(), 200);
		response.headers.SetHeader("X-Powered-By", MODNAME);
		response.headers.SetHeader("Content-Type", "text/xml");
		API->SendResponse(response);
		return MOD_RES_DENY; // Handled
	}
};

MODULE_INIT(ModuleHttpStats)
