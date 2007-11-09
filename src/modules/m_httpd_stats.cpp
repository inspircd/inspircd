/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "httpd.h"

/* $ModDesc: Provides statistics over HTTP via m_httpd.so */
/* $ModDep: httpd.h */

typedef std::map<irc::string,int> StatsHash;
typedef StatsHash::iterator StatsIter;

typedef std::vector<std::pair<int,irc::string> > SortedList;
typedef SortedList::iterator SortedIter;

static StatsHash* sh = new StatsHash();
static SortedList* so = new SortedList();

static StatsHash* Servers = new StatsHash();

class ModuleHttpStats : public Module
{
	
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
		Implementation eventlist[] = { I_OnEvent, I_OnRequest, I_OnChannelDelete, I_OnUserJoin, I_OnUserPart, I_OnUserQuit };
		ServerInstance->Modules->Attach(eventlist, this, 6);
	}

	void InsertOrder(irc::string channel, int count)
	{
		/* This function figures out where in the sorted list to put an item from the hash */
		SortedIter a;
		for (a = so->begin(); a != so->end(); a++)
		{
			/* Found an item equal to or less than, we insert our item before it */
			if (a->first <= count)
			{
				so->insert(a,std::pair<int,irc::string>(count,channel));
				return;
			}
		}
		/* There are no items in the list yet, insert something at the beginning */
		so->insert(so->begin(), std::pair<int,irc::string>(count,channel));
	}

	void SortList()
	{
		/* Sorts the hash into the sorted list using an insertion sort */
		so->clear();
		for (StatsIter a = sh->begin(); a != sh->end(); a++)
			InsertOrder(a->first, a->second);
		for (user_hash::iterator u = ServerInstance->clientlist->begin(); u != ServerInstance->clientlist->end(); u++)
		{
			StatsHash::iterator n = Servers->find(u->second->server);
			if (n != Servers->end())
			{
				n->second++;
			}
			else
			{
				Servers->insert(std::make_pair<irc::string,int>(u->second->server,1));
			}
		}
		this->changed = false;
	}

	void OnEvent(Event* event)
	{
		std::stringstream data("");

		if (event->GetEventID() == "httpd_url")
		{
			HTTPRequest* http = (HTTPRequest*)event->GetData();

			if ((http->GetURI() == "/stats") || (http->GetURI() == "/stats/"))
			{
				data << "<inspircdstats>";

				data << "<server><name>" << ServerInstance->Config->ServerName << "</name><gecos>" << ServerInstance->Config->ServerDesc << "</gecos></server>";

				data << "<general>";
				data << "<usercount>" << ServerInstance->clientlist->size() << "</usercount>";
				data << "<channelcount>" << ServerInstance->chanlist->size() << "</channelcount>";
				data << "<opercount>" << ServerInstance->all_opers.size() << "</opercount>";
				data << "<socketcount>" << (ServerInstance->SE->GetMaxFds() - ServerInstance->SE->GetRemainingFds()) << "</socketcount><socketmax>" << ServerInstance->SE->GetMaxFds() <<
					"</socketmax><socketengine>" << ServerInstance->SE->GetName() << "</socketengine>";

				time_t current_time = 0;
				current_time = ServerInstance->Time();
				time_t server_uptime = current_time - ServerInstance->startup_time;
				struct tm* stime;
				stime = gmtime(&server_uptime);
				data << "<uptime><days>" << stime->tm_yday << "</days><hours>" << stime->tm_hour << "</hours><mins>" << stime->tm_min << "</mins><secs>" << stime->tm_sec << "</secs></uptime>";


				data << "</general>";
				data << "<modulelist>";
				std::vector<std::string> module_names = ServerInstance->Modules->GetAllModuleNames(0);
				for (std::vector<std::string>::iterator i = module_names.begin(); i != module_names.end(); ++i)
				{
					Module* m = ServerInstance->Modules->Find(i->c_str());
					Version v = m->GetVersion();
					data << "<module><name>" << *i << "</name><version>" << v.Major << "." <<  v.Minor << "." << v.Revision << "." << v.Build << "</version></module>";
				}
				data << "</modulelist>";

				data << "<channellist>";
				/* If the list has changed since last time it was displayed, re-sort it
				 * this time only (not every time, as this would be moronic)
				 */
				if (this->changed)
					this->SortList();

				for (SortedIter a = so->begin(); a != so->end(); a++)
				{
					Channel* c = ServerInstance->FindChan(a->second.c_str());
					if (c && !c->IsModeSet('s') && !c->IsModeSet('p'))
					{
						data << "<channel>";
						data << "<usercount>" << c->GetUsers()->size() << "</usercount><channelname>" << c->name << "</channelname>";
						data << "<channelops>" << c->GetOppedUsers()->size() << "</channelops>";
						data << "<channelhalfops>" << c->GetHalfoppedUsers()->size() << "</channelhalfops>";
						data << "<channelvoices>" << c->GetVoicedUsers()->size() << "</channelvoices>";
						data << "<channeltopic>" << c->topic << "</channeltopic>";
						data << "<channelmodes>" << c->ChanModes(false) << "</channelmodes>";
						data << "</channel>";
					}
				}

				data << "</channellist>";

				data << "<serverlist>";
				
				for (StatsHash::iterator b = Servers->begin(); b != Servers->end(); b++)
				{
					data << "<server>";
					data << "<servername>" << b->first << "</servername>";
					data << "<usercount>" << b->second << "</usercount>";
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

	void OnChannelDelete(Channel* chan)
	{
		StatsIter a = sh->find(chan->name);
		if (a != sh->end())
		{
			sh->erase(a);
		}
		this->changed = true;
	}

	void OnUserJoin(User* user, Channel* channel, bool sync, bool &silent)
	{
		StatsIter a = sh->find(channel->name);
		if (a != sh->end())
		{
			a->second++;
		}
		else
		{
			irc::string name = channel->name;
			sh->insert(std::pair<irc::string,int>(name,1));
		}
		this->changed = true;
	}

	void OnUserPart(User* user, Channel* channel, const std::string &partmessage, bool &silent)
	{
		StatsIter a = sh->find(channel->name);
		if (a != sh->end())
		{
			a->second--;
		}
		this->changed = true;
	}

	void OnUserQuit(User* user, const std::string &message, const std::string &oper_message)
	{
		for (UCListIter v = user->chans.begin(); v != user->chans.end(); v++)
		{
			Channel* c = v->first;
			StatsIter a = sh->find(c->name);
			if (a != sh->end())
			{
				a->second--;
			}
		}
		this->changed = true;
	}

	char* OnRequest(Request* request)
	{
		return NULL;
	}


	virtual ~ModuleHttpStats()
	{
		delete sh;
		delete so;
	}

	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleHttpStats)
