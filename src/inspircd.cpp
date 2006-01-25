/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* Now with added unF! ;) */

using namespace std;

#include "inspircd_config.h"
#include "inspircd.h"
#include "inspircd_io.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <time.h>
#include <string>
#ifdef GCC3
#include <ext/hash_map>
#else
#include <hash_map>
#endif
#include <map>
#include <sstream>
#include <vector>
#include <deque>
#ifdef THREADED_DNS
#include <pthread.h>
#endif
#include "users.h"
#include "ctables.h"
#include "globals.h"
#include "modules.h"
#include "dynamic.h"
#include "wildcard.h"
#include "message.h"
#include "mode.h"
#include "commands.h"
#include "xline.h"
#include "inspstring.h"
#include "dnsqueue.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "socketengine.h"
#include "userprocess.h"
#include "socket.h"
#include "typedefs.h"
#include "command_parse.h"

InspIRCd* ServerInstance;

int WHOWAS_STALE = 48; // default WHOWAS Entries last 2 days before they go 'stale'
int WHOWAS_MAX = 100;  // default 100 people maximum in the WHOWAS list

extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;

std::vector<InspSocket*> module_sockets;
std::vector<userrec*> local_users;

extern int MODCOUNT;
int openSockfd[MAXSOCKS];
sockaddr_in client,server;
socklen_t length;

extern InspSocket* socket_ref[MAX_DESCRIPTORS];

time_t TIME = time(NULL), OLDTIME = time(NULL);

// This table references users by file descriptor.
// its an array to make it VERY fast, as all lookups are referenced
// by an integer, meaning there is no need for a scan/search operation.
userrec* fd_ref_table[MAX_DESCRIPTORS];

Server* MyServer = new Server;
ServerConfig *Config = new ServerConfig;

user_hash clientlist;
chan_hash chanlist;
whowas_hash whowas;
servernamelist servernames;
char lowermap[255];

void AddServerName(std::string servername)
{
	log(DEBUG,"Adding server name: %s",servername.c_str());
	for (servernamelist::iterator a = servernames.begin(); a < servernames.end(); a++)
	{
		if (*a == servername)
			return;
	}
	servernames.push_back(servername);
}

const char* FindServerNamePtr(std::string servername)
{
	for (servernamelist::iterator a = servernames.begin(); a < servernames.end(); a++)
	{
		if (*a == servername)
			return a->c_str();
	}
	AddServerName(servername);
	return FindServerNamePtr(servername);
}

std::string InspIRCd::GetRevision()
{
	/* w00t got me to replace a bunch of strtok_r
	 * with something nicer, so i did this. Its the
	 * same thing really, only in C++. It places the
	 * text into a std::stringstream which is a readable
	 * and writeable buffer stream, and then pops two
	 * words off it, space delimited. Because it reads
	 * into the same variable twice, the first word
	 * is discarded, and the second one returned.
	 */
	std::stringstream Revision("$Revision$");
	std::string single;
	Revision >> single >> single;
	return single;
}

void InspIRCd::MakeLowerMap()
{
	// initialize the lowercase mapping table
	for (unsigned int cn = 0; cn < 256; cn++)
		lowermap[cn] = cn;
	// lowercase the uppercase chars
	for (unsigned int cn = 65; cn < 91; cn++)
		lowermap[cn] = tolower(cn);
	// now replace the specific chars for scandanavian comparison
	lowermap[(unsigned)'['] = '{';
	lowermap[(unsigned)']'] = '}';
	lowermap[(unsigned)'\\'] = '|';
}

InspIRCd::InspIRCd(int argc, char** argv)
{
	Start();
	module_sockets.clear();
	this->startup_time = time(NULL);
	srand(time(NULL));
	log(DEBUG,"*** InspIRCd starting up!");
	if (!FileExists(CONFIG_FILE))
	{
		printf("ERROR: Cannot open config file: %s\nExiting...\n",CONFIG_FILE);
		log(DEFAULT,"main: no config");
		printf("ERROR: Your config file is missing, this IRCd will self destruct in 10 seconds!\n");
		Exit(ERROR);
	}
	if (argc > 1) {
		for (int i = 1; i < argc; i++)
		{
			if (!strcmp(argv[i],"-nofork")) {
				Config->nofork = true;
			}
			if (!strcmp(argv[i],"-wait")) {
				sleep(6);
			}
			if (!strcmp(argv[i],"-nolimit")) {
				Config->unlimitcore = true;
			}
		}
	}

	strlcpy(Config->MyExecutable,argv[0],MAXBUF);

	this->MakeLowerMap();

        OpenLog(argv, argc);
        Config->ClearStack();
        Config->Read(true,NULL);
        CheckRoot();
	this->ModeGrok = new ModeParser();
	this->Parser = new CommandParser();
	this->stats = new serverstats();
        AddServerName(Config->ServerName);
        CheckDie();
        stats->BoundPortCount = BindPorts();

	for(int t = 0; t < 255; t++)
		Config->global_implementation[t] = 0;

	memset(&Config->implement_lists,0,sizeof(Config->implement_lists));

        printf("\n");
	SetSignals();
        if (!Config->nofork)
        {
                if (DaemonSeed() == ERROR)
                {
                        printf("ERROR: could not go into daemon mode. Shutting down.\n");
                        Exit(ERROR);
                }
        }

        /* Because of limitations in kqueue on freebsd, we must fork BEFORE we
         * initialize the socket engine.
         */
        SE = new SocketEngine();

        /* We must load the modules AFTER initializing the socket engine, now */

	return;
}

std::string InspIRCd::GetVersionString()
{
	char versiondata[MAXBUF];
#ifdef THREADED_DNS
	char dnsengine[] = "multithread";
#else
	char dnsengine[] = "singlethread";
#endif
	if (*Config->CustomVersion)
	{
		snprintf(versiondata,MAXBUF,"%s Rev. %s %s :%s",VERSION,GetRevision().c_str(),Config->ServerName,Config->CustomVersion);
	}
	else
	{
		snprintf(versiondata,MAXBUF,"%s Rev. %s %s :%s [FLAGS=%lu,%s,%s]",VERSION,GetRevision().c_str(),Config->ServerName,SYSTEM,(unsigned long)OPTIMISATION,SE->GetName().c_str(),dnsengine);
	}
	return versiondata;
}

char* InspIRCd::ModuleError()
{
	return MODERR;
}

void InspIRCd::erase_factory(int j)
{
	int v = 0;
	for (std::vector<ircd_module*>::iterator t = factory.begin(); t != factory.end(); t++)
	{
		if (v == j)
		{
                	factory.erase(t);
                 	factory.push_back(NULL);
                 	return;
           	}
		v++;
     	}
}

void InspIRCd::erase_module(int j)
{
	int v1 = 0;
	for (std::vector<Module*>::iterator m = modules.begin(); m!= modules.end(); m++)
        {
                if (v1 == j)
                {
			delete *m;
                        modules.erase(m);
                        modules.push_back(NULL);
			break;
                }
		v1++;
        }
	int v2 = 0;
        for (std::vector<std::string>::iterator v = Config->module_names.begin(); v != Config->module_names.end(); v++)
        {
                if (v2 == j)
                {
                       Config->module_names.erase(v);
                       break;
                }
		v2++;
        }

}

void InspIRCd::MoveTo(std::string modulename,int slot)
{
	unsigned int v2 = 256;
	log(DEBUG,"Moving %s to slot %d",modulename.c_str(),slot);
	for (unsigned int v = 0; v < Config->module_names.size(); v++)
	{
		if (Config->module_names[v] == modulename)
		{
			// found an instance, swap it with the item at MODCOUNT
			v2 = v;
			break;
		}
	}
	if (v2 == (unsigned int)slot)
	{
		log(DEBUG,"Item %s already in slot %d!",modulename.c_str(),slot);
	}
	else if (v2 < 256)
	{
		// Swap the module names over
		Config->module_names[v2] = Config->module_names[slot];
		Config->module_names[slot] = modulename;
		// now swap the module factories
		ircd_module* temp = factory[v2];
		factory[v2] = factory[slot];
		factory[slot] = temp;
		// now swap the module objects
		Module* temp_module = modules[v2];
		modules[v2] = modules[slot];
		modules[slot] = temp_module;
		// now swap the implement lists (we dont
		// need to swap the global or recount it)
		for (int n = 0; n < 255; n++)
		{
			char x = Config->implement_lists[v2][n];
			Config->implement_lists[v2][n] = Config->implement_lists[slot][n];
			Config->implement_lists[slot][n] = x;
		}
		log(DEBUG,"Moved %s to slot successfully",modulename.c_str());
	}
	else
	{
		log(DEBUG,"Move of %s to slot failed!",modulename.c_str());
	}
}

void InspIRCd::MoveAfter(std::string modulename, std::string after)
{
	log(DEBUG,"Move %s after %s...",modulename.c_str(),after.c_str());
	for (unsigned int v = 0; v < Config->module_names.size(); v++)
	{
		log(DEBUG,"Curr=%s after=%s v=%d",Config->module_names[v].c_str(),after.c_str(),v);
		if (Config->module_names[v] == after)
		{
			MoveTo(modulename, v);
			return;
		}
	}
}

void InspIRCd::MoveBefore(std::string modulename, std::string before)
{
	log(DEBUG,"Move %s before %s...",modulename.c_str(),before.c_str());
	for (unsigned int v = 0; v < Config->module_names.size(); v++)
	{
		if (Config->module_names[v] == before)
		{
			if (v > 0)
			{
				MoveTo(modulename, v-1);
			}
			else
			{
				MoveTo(modulename, v);
			}
			return;
		}
	}
}

void InspIRCd::MoveToFirst(std::string modulename)
{
	MoveTo(modulename,0);
}

void InspIRCd::MoveToLast(std::string modulename)
{
	MoveTo(modulename,MODCOUNT);
}

void InspIRCd::BuildISupport()
{
        // the neatest way to construct the initial 005 numeric, considering the number of configure constants to go in it...
	std::stringstream v;
	v << "WALLCHOPS MODES=" << MAXMODES << " CHANTYPES=# PREFIX=(ohv)@%+ MAP SAFELIST MAXCHANNELS=" << MAXCHANS << " MAXBANS=60 NICKLEN=" << NICKMAX-1;
	v << " CASEMAPPING=rfc1459 STATUSMSG=@+ CHARSET=ascii TOPICLEN=" << MAXTOPIC << " KICKLEN=" << MAXKICK << " MAXTARGETS=" << Config->MaxTargets << " AWAYLEN=";
	v << MAXAWAY << " CHANMODES=b,k,l,psmnti NETWORK=" << Config->Network;
	Config->data005 = v.str();
	FOREACH_MOD(I_On005Numeric,On005Numeric(Config->data005));
}

bool InspIRCd::UnloadModule(const char* filename)
{
	std::string filename_str = filename;
	for (unsigned int j = 0; j != Config->module_names.size(); j++)
	{
		if (Config->module_names[j] == filename_str)
		{
			if (modules[j]->GetVersion().Flags & VF_STATIC)
			{
				log(DEFAULT,"Failed to unload STATIC module %s",filename);
				snprintf(MODERR,MAXBUF,"Module not unloadable (marked static)");
				return false;
			}
			/* Give the module a chance to tidy out all its metadata */
			for (chan_hash::iterator c = chanlist.begin(); c != chanlist.end(); c++)
			{
				modules[j]->OnCleanup(TYPE_CHANNEL,c->second);
			}
			for (user_hash::iterator u = clientlist.begin(); u != clientlist.end(); u++)
			{
				modules[j]->OnCleanup(TYPE_USER,u->second);
			}

			FOREACH_MOD(I_OnUnloadModule,OnUnloadModule(modules[j],Config->module_names[j]));

                        for(int t = 0; t < 255; t++)
			{
				Config->global_implementation[t] -= Config->implement_lists[j][t];
			}

			/* We have to renumber implement_lists after unload because the module numbers change!
			 */
			for(int j2 = j; j2 < 254; j2++)
			{
				for(int t = 0; t < 255; t++)
				{
					Config->implement_lists[j2][t] = Config->implement_lists[j2+1][t];
				}
			}

			// found the module
			log(DEBUG,"Deleting module...");
			erase_module(j);
			log(DEBUG,"Erasing module entry...");
			erase_factory(j);
                        log(DEBUG,"Removing dependent commands...");
                        Parser->RemoveCommands(filename);
			log(DEFAULT,"Module %s unloaded",filename);
			MODCOUNT--;
			BuildISupport();
			return true;
		}
	}
	log(DEFAULT,"Module %s is not loaded, cannot unload it!",filename);
	snprintf(MODERR,MAXBUF,"Module not loaded");
	return false;
}

bool InspIRCd::LoadModule(const char* filename)
{
	char modfile[MAXBUF];
#ifdef STATIC_LINK
	strlcpy(modfile,filename,MAXBUF);
#else
	snprintf(modfile,MAXBUF,"%s/%s",Config->ModPath,filename);
#endif
	std::string filename_str = filename;
#ifndef STATIC_LINK
	if (!DirValid(modfile))
	{
		log(DEFAULT,"Module %s is not within the modules directory.",modfile);
		snprintf(MODERR,MAXBUF,"Module %s is not within the modules directory.",modfile);
		return false;
	}
#endif
	log(DEBUG,"Loading module: %s",modfile);
#ifndef STATIC_LINK
        if (FileExists(modfile))
        {
#endif
		for (unsigned int j = 0; j < Config->module_names.size(); j++)
		{
			if (Config->module_names[j] == filename_str)
			{
				log(DEFAULT,"Module %s is already loaded, cannot load a module twice!",modfile);
				snprintf(MODERR,MAXBUF,"Module already loaded");
				return false;
			}
		}
		ircd_module* a = new ircd_module(modfile);
                factory[MODCOUNT+1] = a;
                if (factory[MODCOUNT+1]->LastError())
                {
                        log(DEFAULT,"Unable to load %s: %s",modfile,factory[MODCOUNT+1]->LastError());
			snprintf(MODERR,MAXBUF,"Loader/Linker error: %s",factory[MODCOUNT+1]->LastError());
			MODCOUNT--;
			return false;
                }
                if (factory[MODCOUNT+1]->factory)
                {
			Module* m = factory[MODCOUNT+1]->factory->CreateModule(MyServer);
                        modules[MODCOUNT+1] = m;
                        /* save the module and the module's classfactory, if
                         * this isnt done, random crashes can occur :/ */
                        Config->module_names.push_back(filename);

			char* x = &Config->implement_lists[MODCOUNT+1][0];
			for(int t = 0; t < 255; t++)
				x[t] = 0;

			modules[MODCOUNT+1]->Implements(x);

			for(int t = 0; t < 255; t++)
			{
				Config->global_implementation[t] += Config->implement_lists[MODCOUNT+1][t];
				if (Config->implement_lists[MODCOUNT+1][t])
				{
					log(DEBUG,"Add global implementation: %d %d => %d",MODCOUNT+1,t,Config->global_implementation[t]);
				}
			}
                }
		else
                {
                        log(DEFAULT,"Unable to load %s",modfile);
			snprintf(MODERR,MAXBUF,"Factory function failed!");
			return false;
                }
#ifndef STATIC_LINK
        }
        else
        {
                log(DEFAULT,"InspIRCd: startup: Module Not Found %s",modfile);
		snprintf(MODERR,MAXBUF,"Module file could not be found");
		return false;
        }
#endif
	MODCOUNT++;
	FOREACH_MOD(I_OnLoadModule,OnLoadModule(modules[MODCOUNT],filename_str));
	// now work out which modules, if any, want to move to the back of the queue,
	// and if they do, move them there.
	std::vector<std::string> put_to_back;
	std::vector<std::string> put_to_front;
	std::map<std::string,std::string> put_before;
	std::map<std::string,std::string> put_after;
	for (unsigned int j = 0; j < Config->module_names.size(); j++)
	{
		if (modules[j]->Prioritize() == PRIORITY_LAST)
		{
			put_to_back.push_back(Config->module_names[j]);
		}
		else if (modules[j]->Prioritize() == PRIORITY_FIRST)
		{
			put_to_front.push_back(Config->module_names[j]);
		}
		else if ((modules[j]->Prioritize() & 0xFF) == PRIORITY_BEFORE)
		{
			log(DEBUG,"Module %d wants PRIORITY_BEFORE",j);
			put_before[Config->module_names[j]] = Config->module_names[modules[j]->Prioritize() >> 8];
			log(DEBUG,"Before: %s",Config->module_names[modules[j]->Prioritize() >> 8].c_str());
		}
		else if ((modules[j]->Prioritize() & 0xFF) == PRIORITY_AFTER)
		{
			log(DEBUG,"Module %d wants PRIORITY_AFTER",j);
			put_after[Config->module_names[j]] = Config->module_names[modules[j]->Prioritize() >> 8];
			log(DEBUG,"After: %s",Config->module_names[modules[j]->Prioritize() >> 8].c_str());
		}
	}
	for (unsigned int j = 0; j < put_to_back.size(); j++)
	{
		MoveToLast(put_to_back[j]);
	}
	for (unsigned int j = 0; j < put_to_front.size(); j++)
	{
		MoveToFirst(put_to_front[j]);
	}
	for (std::map<std::string,std::string>::iterator j = put_before.begin(); j != put_before.end(); j++)
	{
		MoveBefore(j->first,j->second);
	}
	for (std::map<std::string,std::string>::iterator j = put_after.begin(); j != put_after.end(); j++)
	{
		MoveAfter(j->first,j->second);
	}
	BuildISupport();
	return true;
}

int InspIRCd::Run()
{
	bool expire_run = false;
	int activefds[MAX_DESCRIPTORS];
	int incomingSockfd;
	int in_port;
	userrec* cu = NULL;
	InspSocket* s = NULL;
	InspSocket* s_del = NULL;
	char* target;
	unsigned int numberactive;
        sockaddr_in sock_us;     // our port number
	socklen_t uslen;         // length of our port number

	/* Until THIS point, ServerInstance == NULL */
	
        LoadAllModules(this);

        printf("\nInspIRCd is now running!\n");
	
	if (!Config->nofork)
	{
		freopen("/dev/null","w",stdout);
		freopen("/dev/null","w",stderr);
	}

	/* Add the listening sockets used for client inbound connections
	 * to the socket engine
	 */
	for (int count = 0; count < stats->BoundPortCount; count++)
		SE->AddFd(openSockfd[count],true,X_LISTEN);

	WritePID(Config->PID);

	/* main loop, this never returns */
	for (;;)
	{
		/* time() seems to be a pretty expensive syscall, so avoid calling it too much.
		 * Once per loop iteration is pleanty.
		 */
		OLDTIME = TIME;
		TIME = time(NULL);

		/* Run background module timers every few seconds
		 * (the docs say modules shouldnt rely on accurate
		 * timing using this event, so we dont have to
		 * time this exactly).
		 */
		if (((TIME % 8) == 0) && (!expire_run))
		{
			expire_lines();
			FOREACH_MOD(I_OnBackgroundTimer,OnBackgroundTimer(TIME));
			expire_run = true;
			continue;
		}
		else if ((TIME % 8) == 1)
		{
			expire_run = false;
		}
		
		/* Once a second, do the background processing */
		if (TIME != OLDTIME)
			DoBackgroundUserStuff(TIME);

		/* Call the socket engine to wait on the active
		 * file descriptors. The socket engine has everything's
		 * descriptors in its list... dns, modules, users,
		 * servers... so its nice and easy, just one call.
		 */
		if (!(numberactive = SE->Wait(activefds)))
			continue;

		/**
		 * Now process each of the fd's. For users, we have a fast
		 * lookup table which can find a user by file descriptor, so
		 * processing them by fd isnt expensive. If we have a lot of
		 * listening ports or module sockets though, things could get
		 * ugly.
		 */
		for (unsigned int activefd = 0; activefd < numberactive; activefd++)
		{
			int socket_type = SE->GetType(activefds[activefd]);
			switch (socket_type)
			{
				case X_ESTAB_CLIENT:

					cu = fd_ref_table[activefds[activefd]];
					if (cu)
						ProcessUser(cu);

				break;

				case X_ESTAB_MODULE:

					/* Process module-owned sockets.
					 * Modules are encouraged to inherit their sockets from
					 * InspSocket so we can process them neatly like this.
					 */
					s = socket_ref[activefds[activefd]];

					if ((s) && (!s->Poll()))
					{
						log(DEBUG,"Socket poll returned false, close and bail");
						SE->DelFd(s->GetFd());
						for (std::vector<InspSocket*>::iterator a = module_sockets.begin(); a < module_sockets.end(); a++)
						{
							s_del = (InspSocket*)*a;
							if ((s_del) && (s_del->GetFd() == activefds[activefd]))
							{
								module_sockets.erase(a);
								break;
							}
						}
						s->Close();
						delete s;
					}

				break;

				case X_ESTAB_DNS:

					/* When we are using single-threaded dns,
					 * the sockets for dns end up in our mainloop.
					 * When we are using multi-threaded dns,
					 * each thread has its own basic poll() loop
					 * within it, making them 'fire and forget'
					 * and independent of the mainloop.
					 */
#ifndef THREADED_DNS
					dns_poll(activefds[activefd]);
#endif
				break;
				
				case X_LISTEN:

					/* It's a listener */
					uslen = sizeof(sock_us);
					length = sizeof(client);
					incomingSockfd = accept (activefds[activefd],(struct sockaddr*)&client,&length);
					
					if ((incomingSockfd > -1) && (!getsockname(incomingSockfd,(sockaddr*)&sock_us,&uslen)))
					{
						in_port = ntohs(sock_us.sin_port);
						log(DEBUG,"Accepted socket %d",incomingSockfd);
						target = (char*)inet_ntoa(client.sin_addr);
						/* Years and years ago, we used to resolve here
						 * using gethostbyaddr(). That is sucky and we
						 * don't do that any more...
						 */
						NonBlocking(incomingSockfd);
						if (Config->GetIOHook(in_port))
						{
							Config->GetIOHook(in_port)->OnRawSocketAccept(incomingSockfd, target, in_port);
						}
						stats->statsAccept++;
						AddClient(incomingSockfd, target, in_port, false, target);
						log(DEBUG,"Adding client on port %lu fd=%lu",(unsigned long)in_port,(unsigned long)incomingSockfd);
					}
					else
					{
						log(DEBUG,"Accept failed on fd %lu: %s",(unsigned long)incomingSockfd,strerror(errno));
						shutdown(incomingSockfd,2);
						close(incomingSockfd);
						stats->statsRefused++;
					}
				break;

				default:
					/* Something went wrong if we're in here.
					 * In fact, so wrong, im not quite sure
					 * what we would do, so for now, its going
					 * to safely do bugger all.
					 */
				break;
			}
		}

	}
	/* This is never reached -- we hope! */
	return 0;
}

/**********************************************************************************/

/**
 * An ircd in four lines! bwahahaha. ahahahahaha. ahahah *cough*.
 */

int main(int argc, char** argv)
{
        ServerInstance = new InspIRCd(argc, argv);
        ServerInstance->Run();
        delete ServerInstance;
        return 0;
}

