/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *		       E-mail:
 *		<brain@chatspike.net>
 *	   	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include <stdio.h>
#include "users.h"
#include "channels.h"
#include "configreader.h"
#include "modules.h"
#include "inspsocket.h"
#include "httpd.h"
#include "inspircd.h"

/* $ModDesc: Provides statistics over HTTP via m_httpd.so */

typedef std::map<irc::string,int> StatsHash;
typedef StatsHash::iterator StatsIter;

typedef std::vector<std::pair<int,irc::string> > SortedList;
typedef SortedList::iterator SortedIter;

static StatsHash* sh = new StatsHash();
static SortedList* so = new SortedList();

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

	ModuleHttpStats(InspIRCd* Me) : Module::Module(Me)
	{
		
		ReadConfig();
		this->changed = false;
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
				data << "<!DOCTYPE html PUBLIC \
					\"-//W3C//DTD XHTML 1.1//EN\" \
					\"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n\
					<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\">";

				data << "<head>";
				data << "<link rel='stylesheet' href='" << this->stylesheet << "' type='text/css' />";
				data << "<title>InspIRCd server statisitics for " << ServerInstance->Config->ServerName << " (" << ServerInstance->Config->ServerDesc << ")</title>";
				data << "</head><body>";
				data << "<h1>InspIRCd server statisitics for " << ServerInstance->Config->ServerName << " (" << ServerInstance->Config->ServerDesc << ")</h1>";

				data << "<div class='totals'>";
				data << "<h2>Totals</h2>";
				data << "<table>";
				data << "<tr><td>Users</td><td>" << ServerInstance->clientlist.size() << "</td></tr>";
				data << "<tr><td>Channels</td><td>" << ServerInstance->chanlist.size() << "</td></tr>";
				data << "<tr><td>Opers</td><td>" << ServerInstance->all_opers.size() << "</td></tr>";
				data << "<tr><td>Sockets</td><td>" << (ServerInstance->SE->GetMaxFds() - ServerInstance->SE->GetRemainingFds()) << " (Max: " << ServerInstance->SE->GetMaxFds() << " via socket engine '" << ServerInstance->SE->GetName() << "')</td></tr>";
				data << "</table>";
				data << "</div>";

				data << "<div class='modules'>";
				data << "<h2>Modules</h2>";
				data << "<table>";
				for (int i = 0; i <= ServerInstance->GetModuleCount(); i++)
				{
					if (ServerInstance->Config->module_names[i] != "")
						data << "<tr><td>" << ServerInstance->Config->module_names[i] << "</td></tr>";
				}
				data << "</table>";
				data << "</div>";

				data << "<div class='channels'>";
				data << "<h2>Channels</h2>";
				data << "<table>";
				data << "<tr><th>Users</th><th>Name</th><th>@</th><th>%</th><th>+</th><th>Topic</th></tr>";

				/* If the list has changed since last time it was displayed, re-sort it
				 * this time only (not every time, as this would be moronic)
				 */
				if (this->changed)
					this->SortList();

				int n = 0;
				for (SortedIter a = so->begin(); ((a != so->end()) && (n < 25)); a++, n++)
				{
					chanrec* c = ServerInstance->FindChan(a->second.c_str());
					if (c)
					{
						data << "<tr><td>" << a->first << "</td><td>" << a->second << "</td>";
						data << "<td>" << c->GetOppedUsers()->size() << "</td>";
						data << "<td>" << c->GetHalfoppedUsers()->size() << "</td>";
						data << "<td>" << c->GetVoicedUsers()->size() << "</td>";
						data << "<td>" << c->topic << "</td>";
						data << "</tr>";
					}
				}

				data << "</table>";
				data << "</div>";





				data << "<div class='validion'>";
				data << "<p><a href='http://validator.w3.org/check?uri=referer'><img src='http://www.w3.org/Icons/valid-xhtml11' alt='Valid XHTML 1.1' height='31' width='88' /></a></p>";
				data << "</div>";
				
				data << "</body>";
				data << "</html>";

				/* Send the document back to m_httpd */
				HTTPDocument response(http->sock, &data, 200, "X-Powered-By: m_http_stats.so\r\nContent-Type: text/html; charset=iso-8859-1\r\n");
				Request req((char*)&response, (Module*)this, event->GetSource());
				req.Send();

				ServerInstance->Log(DEBUG,"Sent");
			}
		}
	}

	void OnChannelDelete(chanrec* chan)
	{
		StatsIter a = sh->find(chan->name);
		if (a != sh->end())
		{
			sh->erase(a);
		}
		this->changed = true;
	}

	void OnUserJoin(userrec* user, chanrec* channel)
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

	void OnUserPart(userrec* user, chanrec* channel, const std::string &partmessage)
	{
		StatsIter a = sh->find(channel->name);
		if (a != sh->end())
		{
			a->second--;
		}
		this->changed = true;
	}

	void OnUserQuit(userrec* user, const std::string &message)
	{
		for (std::vector<ucrec*>::const_iterator v = user->chans.begin(); v != user->chans.end(); v++)
		{
			if (((ucrec*)(*v))->channel)
			{
				chanrec* c = ((ucrec*)(*v))->channel;
				StatsIter a = sh->find(c->name);
				if (a != sh->end())
				{
					a->second--;
				}
			}
		}
		this->changed = true;
	}

	char* OnRequest(Request* request)
	{
		return NULL;
	}

	void Implements(char* List)
	{
		List[I_OnEvent] = List[I_OnRequest] = List[I_OnChannelDelete] = List[I_OnUserJoin] = List[I_OnUserPart] = List[I_OnUserQuit] = 1;
	}

	virtual ~ModuleHttpStats()
	{
		delete sh;
		delete so;
	}

	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_VENDOR, API_VERSION);
	}
};


class ModuleHttpStatsFactory : public ModuleFactory
{
 public:
	ModuleHttpStatsFactory()
	{
	}
	
	~ModuleHttpStatsFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleHttpStats(Me);
	}
};


extern "C" void * init_module( void )
{
	return new ModuleHttpStatsFactory;
}
