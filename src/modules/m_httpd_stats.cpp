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
#include "helperfuncs.h"
#include "httpd.h"
#include "inspircd.h"

/* $ModDesc: Provides statistics over HTTP via m_httpd.so */

extern user_hash clientlist;
extern chan_hash chanlist;
extern std::vector<userrec*> all_opers;
extern InspIRCd* ServerInstance;
extern ServerConfig* Config;

extern int MODCOUNT;

typedef std::map<irc::string,int> StatsHash;
typedef StatsHash::iterator StatsIter;

typedef std::vector<std::pair<int,irc::string> > SortedList;
typedef SortedList::iterator SortedIter;

static StatsHash* sh = new StatsHash();
static SortedList* so = new SortedList();

class ModuleHttpStats : public Module
{
	Server* Srv;
	std::string stylesheet;
	bool changed;

 public:

	void ReadConfig()
	{
		ConfigReader c;
		this->stylesheet = c.ReadValue("httpstats", "stylesheet", 0);
	}

	ModuleHttpStats(Server* Me) : Module::Module(Me)
	{
		Srv = Me;
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
		{
			log(DEBUG, "InsertOrder on %d %s",a->second,a->first.c_str());
			InsertOrder(a->first, a->second);
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
				log(DEBUG,"HTTP URL!");

				data << "<HTML><HEAD>";
				data << "<TITLE>InspIRCd server statisitics for " << Srv->GetServerName() << " (" << Srv->GetServerDescription() << ")</TITLE>";
				data << "</HEAD><BODY>";
				data << "<H1>InspIRCd server statisitics for " << Srv->GetServerName() << " (" << Srv->GetServerDescription() << ")</H1>";

				data << "<DIV ID='TOTALS'>";
				data << "<H2>Totals</H2>";
				data << "<TABLE>";
				data << "<TR><TD>Users</TD><TD>" << clientlist.size() << "</TD></TR>";
				data << "<TR><TD>Channels</TD><TD>" << chanlist.size() << "</TD></TR>";
				data << "<TR><TD>Opers</TD><TD>" << all_opers.size() << "</TD></TR>";
				data << "<TR><TD>Sockets</TD><TD>" << (ServerInstance->SE->GetMaxFds() - ServerInstance->SE->GetRemainingFds()) << " (Max: " << ServerInstance->SE->GetMaxFds() << " via socket engine '" << ServerInstance->SE->GetName() << "')</TD></TR>";
				data << "</TABLE>";
				data << "</DIV>";

				data << "<DIV ID='MODULES'>";
				data << "<H2>Modules</H2>";
				data << "<TABLE>";
				for (int i = 0; i <= MODCOUNT; i++)
				{
					if (Config->module_names[i] != "")
						data << "<TR><TD>" << Config->module_names[i] << "</TD></TR>";
				}
				data << "</TABLE>";
				data << "</DIV>";

				data << "<DIV ID='CHANNELS'>";
				data << "<H2>Channels</H2>";
				data << "<TABLE>";
				data << "<TR><TH>Users</TH><TH>Count</TH></TR>";

				/* If the list has changed since last time it was displayed, re-sort it
				 * this time only (not every time, as this would be moronic)
				 */
				if (this->changed)
					this->SortList();

				int n = 0;
				for (SortedIter a = so->begin(); ((a != so->end()) && (n < 25)); a++, n++)
				{
					data << "<TR><TD>" << a->first << "</TD><TD>" << a->second << "</TD></TR>";
				}

				data << "</TABLE>";
				data << "</DIV>";

				
				data << "</BODY>";
				data << "</HTML>";

				/* Send the document back to m_httpd */
				HTTPDocument response(http->sock, &data, 200, "X-Powered-By: m_http_stats.so\r\nContent-Type: text/html\r\n");
				Request req((char*)&response, (Module*)this, event->GetSource());
				req.Send();

				log(DEBUG,"Sent");
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
	}

	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
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
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleHttpStats(Me);
	}
};


extern "C" void * init_module( void )
{
	return new ModuleHttpStatsFactory;
}
