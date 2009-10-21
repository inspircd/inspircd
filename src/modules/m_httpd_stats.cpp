/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *          the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "httpd.h"
#include "protocol.h"

/* $ModDesc: Provides statistics over HTTP via m_httpd.so */

class ModuleHttpStats : public Module
{
	static std::map<char, char const*> const &entities;
	std::string stylesheet;
	bool changed;

 public:

	void ReadConfig()
	{
		ConfigReader c;
		this->stylesheet = c.ReadValue("httpstats", "stylesheet", 0);
	}

	ModuleHttpStats() 	{
		ReadConfig();
		this->changed = true;
		Implementation eventlist[] = { I_OnEvent };
		ServerInstance->Modules->Attach(eventlist, this, 1);
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
			else if (*x < 32 || *x > 126)
			{
				int n = (unsigned char)*x;
				ret += ("&#" + ConvToStr(n) + ";");
			}
			else
			{
				ret += *x;
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
				data << "<meta name=\"" << item->key << "\">" << Sanitize(value) << "</meta>";
			else if (!item->key.empty())
				data << "<meta name=\"" << item->key << "\"/>";
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
				data << "<inspircdstats>";

				data << "<server><name>" << ServerInstance->Config->ServerName << "</name><gecos>"
					<< Sanitize(ServerInstance->Config->ServerDesc) << "</gecos><version>"
					<< Sanitize(ServerInstance->GetVersionString()) << "</version><revision>"
					<< Sanitize(ServerInstance->GetRevision()) << "</revision></server>";

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

				data << "<isupport>" << Sanitize(ServerInstance->Config->data005) << "</isupport></general>";
				data << "<modulelist>";
				std::vector<std::string> module_names = ServerInstance->Modules->GetAllModuleNames(0);

				for (std::vector<std::string>::iterator i = module_names.begin(); i != module_names.end(); ++i)
				{
					Module* m = ServerInstance->Modules->Find(i->c_str());
					Version v = m->GetVersion();
					data << "<module><name>" << *i << "</name><version>" << v.version << "</version><description>" << Sanitize(v.description) << "</description></module>";
				}
				data << "</modulelist>";
				data << "<channellist>";

				for (chan_hash::const_iterator a = ServerInstance->chanlist->begin(); a != ServerInstance->chanlist->end(); ++a)
				{
					Channel* c = a->second;

					data << "<channel>";
					data << "<usercount>" << c->GetUsers()->size() << "</usercount><channelname>" << c->name << "</channelname>";
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
						data << "<opertype>" << Sanitize(u->oper) << "</opertype>";
					data << "<modes>" << u->FormatModes() << "</modes><ident>" << Sanitize(u->ident) << "</ident>";
					LocalUser* lu = IS_LOCAL(u);
					if (lu)
						data << "<port>" << lu->GetServerPort() << "</port><servaddr>"
							<< irc::sockets::satouser(&lu->server_sa) << "</servaddr>";
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
					data << "<gecos>" << b->gecos << "</gecos>";
					data << "<usercount>" << b->usercount << "</usercount>";
// This is currently not implemented, so, commented out.
//					data << "<opercount>" << b->opercount << "</opercount>";
					data << "<lagmillisecs>" << b->latencyms << "</lagmillisecs>";
					data << "</server>";
				}

				data << "</serverlist>";

				data << "</inspircdstats>";

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
