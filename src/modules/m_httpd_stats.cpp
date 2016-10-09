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
#include "httpd.h"
#include "xline.h"
#include "protocol.h"

/* $ModDesc: Provides statistics over HTTP via m_httpd.so */

class ModuleHttpStats : public Module
{
	static std::map<char, char const*> const &entities;

 public:

	void init()
	{
		Implementation eventlist[] = { I_OnEvent };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	std::string Sanitize(const std::string &str)
	{
		std::string ret;
		ret.reserve(str.length() * 2);

		for (std::string::const_iterator x = str.begin(); x != str.end(); ++x)
		{
			std::map<char, char const*>::const_iterator it = entities.find(*x);

			if (it != entities.end())
			{
				ret += '&';
				ret += it->second;
				ret += ';';
			}
			else if (*x == 0x09 ||  *x == 0x0A || *x == 0x0D || ((*x >= 0x20) && (*x <= 0x7e)))
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

	void DumpMeta(std::stringstream& data, Extensible* ext)
	{
		data << "<metadata>";
		for(Extensible::ExtensibleStore::const_iterator i = ext->GetExtList().begin(); i != ext->GetExtList().end(); i++)
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

	void OnEvent(Event& event)
	{
		std::stringstream data("");

		if (event.id == "httpd_url")
		{
			ServerInstance->Logs->Log("m_http_stats", DEBUG,"Handling httpd event");
			HTTPRequest* http = (HTTPRequest*)&event;

			if ((http->GetURI() == "/stats") || (http->GetURI() == "/stats/"))
			{
				data << "<inspircdstats><server><name>" << ServerInstance->Config->ServerName << "</name><gecos>"
					<< Sanitize(ServerInstance->Config->ServerDesc) << "</gecos><version>"
					<< Sanitize(ServerInstance->GetVersionString()) << "</version></server>";

				data << "<general>";
				data << "<usercount>" << ServerInstance->Users->clientlist->size() << "</usercount>";
				data << "<channelcount>" << ServerInstance->chanlist->size() << "</channelcount>";
				data << "<opercount>" << ServerInstance->Users->all_opers.size() << "</opercount>";
				data << "<socketcount>" << (ServerInstance->SE->GetUsedFds()) << "</socketcount><socketmax>" << ServerInstance->SE->GetMaxFds() << "</socketmax><socketengine>" << ServerInstance->SE->GetName() << "</socketengine>";

				time_t current_time = 0;
				current_time = ServerInstance->Time();
				time_t server_uptime = current_time - ServerInstance->startup_time;
				struct tm* stime;
				stime = gmtime(&server_uptime);
				data << "<uptime><days>" << stime->tm_yday << "</days><hours>" << stime->tm_hour << "</hours><mins>" << stime->tm_min << "</mins><secs>" << stime->tm_sec << "</secs><boot_time_t>" << ServerInstance->startup_time << "</boot_time_t></uptime>";

				data << "<isupport>" << Sanitize(ServerInstance->Config->data005) << "</isupport></general><xlines>";
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

				data << "</xlines><modulelist>";
				std::vector<std::string> module_names = ServerInstance->Modules->GetAllModuleNames(0);

				for (std::vector<std::string>::iterator i = module_names.begin(); i != module_names.end(); ++i)
				{
					Module* m = ServerInstance->Modules->Find(i->c_str());
					Version v = m->GetVersion();
					data << "<module><name>" << *i << "</name><description>" << Sanitize(v.description) << "</description></module>";
				}
				data << "</modulelist><channellist>";

				for (chan_hash::const_iterator a = ServerInstance->chanlist->begin(); a != ServerInstance->chanlist->end(); ++a)
				{
					Channel* c = a->second;

					data << "<channel>";
					data << "<usercount>" << c->GetUsers()->size() << "</usercount><channelname>" << Sanitize(c->name) << "</channelname>";
					data << "<channeltopic>";
					data << "<topictext>" << Sanitize(c->topic) << "</topictext>";
					data << "<setby>" << Sanitize(c->setby) << "</setby>";
					data << "<settime>" << c->topicset << "</settime>";
					data << "</channeltopic>";
					data << "<channelmodes>" << Sanitize(c->ChanModes(true)) << "</channelmodes>";
					const UserMembList* ulist = c->GetUsers();

					for (UserMembCIter x = ulist->begin(); x != ulist->end(); ++x)
					{
						Membership* memb = x->second;
						data << "<channelmember><uid>" << memb->user->uuid << "</uid><privs>"
							<< Sanitize(c->GetAllPrefixChars(x->first)) << "</privs><modes>"
							<< memb->modes << "</modes>";
						DumpMeta(data, memb);
						data << "</channelmember>";
					}

					DumpMeta(data, c);

					data << "</channel>";
				}

				data << "</channellist><userlist>";

				for (user_hash::const_iterator a = ServerInstance->Users->clientlist->begin(); a != ServerInstance->Users->clientlist->end(); ++a)
				{
					User* u = a->second;

					data << "<user>";
					data << "<nickname>" << u->nick << "</nickname><uuid>" << u->uuid << "</uuid><realhost>"
						<< u->host << "</realhost><displayhost>" << u->dhost << "</displayhost><gecos>"
						<< Sanitize(u->fullname) << "</gecos><server>" << u->server << "</server>";
					if (IS_AWAY(u))
						data << "<away>" << Sanitize(u->awaymsg) << "</away><awaytime>" << u->awaytime << "</awaytime>";
					if (IS_OPER(u))
						data << "<opertype>" << Sanitize(u->oper->NameStr()) << "</opertype>";
					data << "<modes>" << u->FormatModes() << "</modes><ident>" << Sanitize(u->ident) << "</ident>";
					LocalUser* lu = IS_LOCAL(u);
					if (lu)
						data << "<port>" << lu->GetServerPort() << "</port><servaddr>"
							<< irc::sockets::satouser(lu->server_sa) << "</servaddr>";
					data << "<ipaddress>" << u->GetIPString() << "</ipaddress>";

					DumpMeta(data, u);

					data << "</user>";
				}

				data << "</userlist><serverlist>";

				ProtoServerList sl;
				ServerInstance->PI->GetServerList(sl);

				for (ProtoServerList::iterator b = sl.begin(); b != sl.end(); ++b)
				{
					data << "<server>";
					data << "<servername>" << b->servername << "</servername>";
					data << "<parentname>" << b->parentname << "</parentname>";
					data << "<gecos>" << Sanitize(b->gecos) << "</gecos>";
					data << "<usercount>" << b->usercount << "</usercount>";
// This is currently not implemented, so, commented out.
//					data << "<opercount>" << b->opercount << "</opercount>";
					data << "<lagmillisecs>" << b->latencyms << "</lagmillisecs>";
					data << "</server>";
				}

				data << "</serverlist></inspircdstats>";

				/* Send the document back to m_httpd */
				HTTPDocumentResponse response(this, *http, &data, 200);
				response.headers.SetHeader("X-Powered-By", "m_httpd_stats.so");
				response.headers.SetHeader("Content-Type", "text/xml");
				response.Send();
			}
		}
	}

	virtual ~ModuleHttpStats()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides statistics over HTTP via m_httpd.so", VF_VENDOR);
	}
};

static std::map<char, char const*> const &init_entities()
{
	static std::map<char, char const*> entities;
	entities['<'] = "lt";
	entities['>'] = "gt";
	entities['&'] = "amp";
	entities['"'] = "quot";
	return entities;
}

std::map<char, char const*> const &ModuleHttpStats::entities = init_entities ();

MODULE_INIT(ModuleHttpStats)
