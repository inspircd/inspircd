/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2005 ChatSpike-Dev.
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
#include <sched.h>
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
#include "dns.h"
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
extern Module* IOHookModule;

extern InspSocket* socket_ref[65535];

time_t TIME = time(NULL), OLDTIME = time(NULL);

SocketEngine* SE = NULL;

// This table references users by file descriptor.
// its an array to make it VERY fast, as all lookups are referenced
// by an integer, meaning there is no need for a scan/search operation.
userrec* fd_ref_table[65536];

serverstats* stats = new serverstats;
Server* MyServer = new Server;
ServerConfig *Config = new ServerConfig;

user_hash clientlist;
chan_hash chanlist;
whowas_hash whowas;
command_table cmdlist;
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


        OpenLog(argv, argc);
        Config->ClearStack();
        Config->Read(true,NULL);
        CheckRoot();
        SetupCommandTable();
        AddServerName(Config->ServerName);
        CheckDie();
        stats->BoundPortCount = BindPorts();

        printf("\n");
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
        LoadAllModules();

        printf("\nInspIRCd is now running!\n");

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
	snprintf(versiondata,MAXBUF,"%s Rev. %s %s :%s [FLAGS=%lu,%s,%s]",VERSION,GetRevision().c_str(),Config->ServerName,SYSTEM,(unsigned long)OPTIMISATION,SE->GetName().c_str(),dnsengine);
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
			FOREACH_MOD OnUnloadModule(modules[j],Config->module_names[j]);
			// found the module
			log(DEBUG,"Deleting module...");
			erase_module(j);
			log(DEBUG,"Erasing module entry...");
			erase_factory(j);
                        log(DEBUG,"Removing dependent commands...");
                        remove_commands(filename);
			log(DEFAULT,"Module %s unloaded",filename);
			MODCOUNT--;
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
	snprintf(modfile,MAXBUF,"%s",filename);
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
	FOREACH_MOD OnLoadModule(modules[MODCOUNT],filename_str);
	return true;
}

int InspIRCd::Run()
{
	bool expire_run = false;
	std::vector<int> activefds;
	int incomingSockfd;
	int in_port;
	userrec* cu = NULL;
	InspSocket* s = NULL;
	InspSocket* s_del = NULL;
	char* target;
	unsigned int numberactive;
        sockaddr_in sock_us;     // our port number
        socklen_t uslen;         // length of our port number

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
			FOREACH_MOD OnBackgroundTimer(TIME);
			expire_run = true;
			continue;
		}
		if ((TIME % 8) == 1)
			expire_run = false;
		
		/* Once a second, do the background processing */
		if (TIME != OLDTIME)
			while (DoBackgroundUserStuff(TIME));

		/* Call the socket engine to wait on the active
		 * file descriptors. The socket engine has everything's
		 * descriptors in its list... dns, modules, users,
		 * servers... so its nice and easy, just one call.
		 */
		SE->Wait(activefds);

		/**
		 * Now process each of the fd's. For users, we have a fast
		 * lookup table which can find a user by file descriptor, so
		 * processing them by fd isnt expensive. If we have a lot of
		 * listening ports or module sockets though, things could get
		 * ugly.
		 */
		numberactive = activefds.size();
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
					if (!getsockname(incomingSockfd,(sockaddr*)&sock_us,&uslen))
					{
						in_port = ntohs(sock_us.sin_port);
						log(DEBUG,"Accepted socket %d",incomingSockfd);
						target = (char*)inet_ntoa(client.sin_addr);
						/* Years and years ago, we used to resolve here
						 * using gethostbyaddr(). That is sucky and we
						 * don't do that any more...
						 */
						if (incomingSockfd >= 0)
						{
							if (IOHookModule)
							{
								IOHookModule->OnRawSocketAccept(incomingSockfd, target, in_port);
							}
							stats->statsAccept++;
							AddClient(incomingSockfd, target, in_port, false, target);
							log(DEBUG,"Adding client on port %lu fd=%lu",(unsigned long)in_port,(unsigned long)incomingSockfd);
						}
						else
						{
							WriteOpers("*** WARNING: accept() failed on port %lu (%s)",(unsigned long)in_port,target);
							log(DEBUG,"accept failed: %lu",(unsigned long)in_port);
							stats->statsRefused++;
						}
					}
					else
					{
						log(DEBUG,"Couldnt look up the port number for fd %lu (OS BROKEN?!)",incomingSockfd);
						shutdown(incomingSockfd,2);
						close(incomingSockfd);
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

