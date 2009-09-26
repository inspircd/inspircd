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
/* $ModDep: httpd.h */

class ModuleHttpStats : public Module
{
	static std::map<char, char const*> const &entities;
	std::string stylesheet;
	bool changed;

 public:

	void ReadConfig()
	{
		ConfigReader c(ServerInstance);
		this->stylesheet = c.ReadValue("httpstats", "stylesheet", 0);
	}

	ModuleHttpStats(InspIRCd* Me) : Module(Me)
	{
		ReadConfig();
		this->changed = true;
		Implementation eventlist[] = { I_OnEvent, I_OnRequest };
		ServerInstance->Modules->Attach(eventlist, this, 2);
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

	void OnEvent(Event* event)
	{
		std::stringstream data("");

		if (event->GetEventID() == "httpd_url")
		{
			ServerInstance->Logs->Log("m_http_stats", DEBUG,"Handling httpd event");
			HTTPRequest* http = (HTTPRequest*)event->GetData();

			if ((http->GetURI() == "/stats") || (http->GetURI() == "/stats/"))
			{
				data << "<inspircdstats>";

				data << "<server><name>" << ServerInstance->Config->ServerName << "</name><gecos>" << Sanitize(ServerInstance->Config->ServerDesc) << "</gecos></server>";

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


				data << "</general>";
				data << "<modulelist>";
				std::vector<std::string> module_names = ServerInstance->Modules->GetAllModuleNames(0);

				for (std::vector<std::string>::iterator i = module_names.begin(); i != module_names.end(); ++i)
				{
					Module* m = ServerInstance->Modules->Find(i->c_str());
					Version v = m->GetVersion();
					data << "<module><name>" << *i << "</name><version>" << v.version << "</version></module>";
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
						data << "<channelmember><uid>" << x->first->uuid << "</uid><privs>" << Sanitize(c->GetAllPrefixChars(x->first)) << "</privs></channelmember>";
					}

					data << "</channel>";
				}

				data << "</channellist><userlist>";

				for (user_hash::const_iterator a = ServerInstance->Users->clientlist->begin(); a != ServerInstance->Users->clientlist->end(); ++a)
				{
					User* u = a->second;

					data << "<user>";
					data << "<nickname>" << u->nick << "</nickname><uuid>" << u->uuid << "</uuid><realhost>" << u->host << "</realhost><displayhost>" << u->dhost << "</displayhost>";
					data << "<gecos>" << Sanitize(u->fullname) << "</gecos><server>" << u->server << "</server><away>" << Sanitize(u->awaymsg) << "</away><opertype>" << Sanitize(u->oper) << "</opertype><modes>";
					std::string modes;
					for (unsigned char n = 'A'; n <= 'z'; ++n)
						if (u->IsModeSet(n))
							modes += n;

					data << modes << "</modes><ident>" << Sanitize(u->ident) << "</ident><port>" << u->GetServerPort() << "</port><ipaddress>" << u->GetIPString() << "</ipaddress>";
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
				HTTPDocument response(http->sock, &data, 200);
				response.headers.SetHeader("X-Powered-By", "m_httpd_stats.so");
				response.headers.SetHeader("Content-Type", "text/xml");
				Request req((char*)&response, (Module*)this, event->GetSource());
				req.Send();
			}
		}
	}

	const char* OnRequest(Request* request)
	{
		return NULL;
	}


	virtual ~ModuleHttpStats()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides statistics over HTTP via m_httpd.so", VF_VENDOR, API_VERSION);
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
