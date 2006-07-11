/* ---------------------------------------------------------------------
 * 
 *              +------------------------------------+
 *              | Inspire Internet Relay Chat Daemon |
 *              +------------------------------------+
 *
 *         InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                             E-mail:
 *                      <brain@chatspike.net>
 *                      <Craig@chatspike.net>
 *     
 *  Written by Craig Edwards, Craig McLure, and others.
 *  This program is free but copyrighted software; you can redistribute
 *  it and/or modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation, version 2
 *  (two) ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * ---------------------------------------------------------------------
 */

#include <algorithm>
#include "inspircd_config.h"
#include "inspircd.h"
#include "configreader.h"
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <time.h>
#include <string>
#include <exception>
#include <stdexcept>
#include <new>
#include <map>
#include <sstream>
#include <fstream>
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

extern ModuleList modules;
extern FactoryList factory;

std::vector<InspSocket*> module_sockets;
std::vector<userrec*> local_users;

extern int MODCOUNT;
extern char LOG_FILE[MAXBUF];
int iterations = 0;

insp_sockaddr client, server;
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

servernamelist servernames;
char lowermap[255];

void AddServerName(const std::string &servername)
{
	log(DEBUG,"Adding server name: %s",servername.c_str());
	
	if(find(servernames.begin(), servernames.end(), servername) == servernames.end())
		servernames.push_back(servername); /* Wasn't already there. */
}

const char* FindServerNamePtr(const std::string &servername)
{
	servernamelist::iterator iter = find(servernames.begin(), servernames.end(), servername);
	
	if(iter == servernames.end())
	{		
		AddServerName(servername);
		iter = --servernames.end();
	}

	return iter->c_str();
}

bool FindServerName(const std::string &servername)
{
	return (find(servernames.begin(), servernames.end(), servername) != servernames.end());
}

void Exit(int status)
{
	if (Config->log_file)
		fclose(Config->log_file);
	send_error("Server shutdown.");
	exit (status);
}

void InspIRCd::Start()
{
	printf("\033[1;32mInspire Internet Relay Chat Server, compiled %s at %s\n",__DATE__,__TIME__);
	printf("(C) ChatSpike Development team.\033[0m\n\n");
	printf("Developers:\t\t\033[1;32mBrain, FrostyCoolSlug, w00t, Om, Special\033[0m\n");
	printf("Others:\t\t\t\033[1;32mSee /INFO Output\033[0m\n");
	printf("Name concept:\t\t\033[1;32mLord_Zathras\033[0m\n\n");
}

void Killed(int status)
{
	if (Config->log_file)
		fclose(Config->log_file);
	send_error("Server terminated.");
	exit(status);
}

void Rehash(int status)
{
	WriteOpers("Rehashing config file %s due to SIGHUP",CleanFilename(CONFIG_FILE));
	fclose(Config->log_file);
	OpenLog(NULL,0);
	Config->Read(false,NULL);
	FOREACH_MOD(I_OnRehash,OnRehash(""));
}

void InspIRCd::SetSignals()
{
	signal (SIGALRM, SIG_IGN);
	signal (SIGHUP, Rehash);
	signal (SIGPIPE, SIG_IGN);
	signal (SIGTERM, Exit);
	signal (SIGSEGV, Error);
}

bool InspIRCd::DaemonSeed()
{
	int childpid;
	if ((childpid = fork ()) < 0)
		return (ERROR);
	else if (childpid > 0)
	{
		/* We wait a few seconds here, so that the shell prompt doesnt come back over the output */
		sleep(6);
		exit (0);
	}
	setsid ();
	umask (007);
	printf("InspIRCd Process ID: \033[1;32m%lu\033[0m\n",(unsigned long)getpid());

	rlimit rl;
	if (getrlimit(RLIMIT_CORE, &rl) == -1)
	{
		log(DEFAULT,"Failed to getrlimit()!");
		return false;
	}
	else
	{
		rl.rlim_cur = rl.rlim_max;
		if (setrlimit(RLIMIT_CORE, &rl) == -1)
			log(DEFAULT,"setrlimit() failed, cannot increase coredump size.");
	}
  
	return true;
}

void InspIRCd::WritePID(const std::string &filename)
{
	std::ofstream outfile(filename.c_str());
	if (outfile.is_open())
	{
		outfile << getpid();
		outfile.close();
	}
	else
	{
		printf("Failed to write PID-file '%s', exiting.\n",filename.c_str());
		log(DEFAULT,"Failed to write PID-file '%s', exiting.",filename.c_str());
		Exit(0);
	}
}

std::string InspIRCd::GetRevision()
{
	return REVISION;
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
	this->Start();
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
	*LOG_FILE = 0;
	if (argc > 1) {
		for (int i = 1; i < argc; i++)
		{
			if (!strcmp(argv[i],"-nofork"))
			{
				Config->nofork = true;
			}
			else if(!strcmp(argv[i],"-debug"))
			{
				Config->forcedebug = true;
			}
			else if(!strcmp(argv[i],"-nolog"))
			{
				Config->writelog = false;
			}
			else if (!strcmp(argv[i],"-wait"))
			{
				sleep(6);
			}
			else if (!strcmp(argv[i],"-nolimit"))
			{
				printf("WARNING: The `-nolimit' option is deprecated, and now on by default. This behaviour may change in the future.\n");
			}
			else if (!strcmp(argv[i],"-logfile"))
			{
				if (argc > i+1)
				{
					strlcpy(LOG_FILE,argv[i+1],MAXBUF);
					printf("LOG: Setting logfile to %s\n",LOG_FILE);
				}
				else
				{
					printf("ERROR: The -logfile parameter must be followed by a log file name and path.\n");
					Exit(ERROR);
				}
			}
		}
	}

	strlcpy(Config->MyExecutable,argv[0],MAXBUF);

	this->MakeLowerMap();

	OpenLog(argv, argc);
	this->stats = new serverstats();
	Config->ClearStack();
	Config->Read(true,NULL);
	CheckRoot();
	this->ModeGrok = new ModeParser();
	this->Parser = new CommandParser();
	AddServerName(Config->ServerName);
	CheckDie();
	stats->BoundPortCount = BindPorts(true);

	for(int t = 0; t < 255; t++)
		Config->global_implementation[t] = 0;

	memset(&Config->implement_lists,0,sizeof(Config->implement_lists));

	printf("\n");
	this->SetSignals();
	if (!Config->nofork)
	{
		if (!this->DaemonSeed())
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
		snprintf(versiondata,MAXBUF,"%s %s :%s",VERSION,Config->ServerName,Config->CustomVersion);
	}
	else
	{
		snprintf(versiondata,MAXBUF,"%s %s :%s [FLAGS=%lu,%s,%s]",VERSION,Config->ServerName,SYSTEM,(unsigned long)OPTIMISATION,SE->GetName().c_str(),dnsengine);
	}
	return versiondata;
}

char* InspIRCd::ModuleError()
{
	return MODERR;
}

void InspIRCd::EraseFactory(int j)
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

void InspIRCd::EraseModule(int j)
{
	int v1 = 0;
	for (std::vector<Module*>::iterator m = modules.begin(); m!= modules.end(); m++)
	{
		if (v1 == j)
		{
			DELETE(*m);
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
	for (unsigned int v = 0; v < Config->module_names.size(); v++)
	{
		if (Config->module_names[v] == modulename)
		{
			// found an instance, swap it with the item at MODCOUNT
			v2 = v;
			break;
		}
	}
	if ((v2 != (unsigned int)slot) && (v2 < 256))
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
	}
	else
	{
		log(DEBUG,"Move of %s to slot failed!",modulename.c_str());
	}
}

void InspIRCd::MoveAfter(std::string modulename, std::string after)
{
	for (unsigned int v = 0; v < Config->module_names.size(); v++)
	{
		if (Config->module_names[v] == after)
		{
			MoveTo(modulename, v);
			return;
		}
	}
}

void InspIRCd::MoveBefore(std::string modulename, std::string before)
{
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
	v << "WALLCHOPS WALLVOICES MODES=" << MAXMODES << " CHANTYPES=# PREFIX=(ohv)@%+ MAP MAXCHANNELS=" << MAXCHANS << " MAXBANS=60 VBANLIST NICKLEN=" << NICKMAX-1;
	v << " CASEMAPPING=rfc1459 STATUSMSG=@%+ CHARSET=ascii TOPICLEN=" << MAXTOPIC << " KICKLEN=" << MAXKICK << " MAXTARGETS=" << Config->MaxTargets << " AWAYLEN=";
	v << MAXAWAY << " CHANMODES=b,k,l,psmnti FNC NETWORK=" << Config->Network << " MAXPARA=32";
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
			this->EraseModule(j);
			log(DEBUG,"Erasing module entry...");
			this->EraseFactory(j);
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
#ifndef IS_CYGWIN
	if (!DirValid(modfile))
	{
		log(DEFAULT,"Module %s is not within the modules directory.",modfile);
		snprintf(MODERR,MAXBUF,"Module %s is not within the modules directory.",modfile);
		return false;
	}
#endif
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
			return false;
		}
		try
		{
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
					Config->global_implementation[t] += Config->implement_lists[MODCOUNT+1][t];
			}
			else
			{
       				log(DEFAULT,"Unable to load %s",modfile);
				snprintf(MODERR,MAXBUF,"Factory function failed!");
				return false;
			}
		}
		catch (ModuleException& modexcept)
		{
			log(DEFAULT,"Unable to load %s: ",modfile,modexcept.GetReason());
			snprintf(MODERR,MAXBUF,"Factory function threw an exception: %s",modexcept.GetReason());
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
			put_before[Config->module_names[j]] = Config->module_names[modules[j]->Prioritize() >> 8];
		}
		else if ((modules[j]->Prioritize() & 0xFF) == PRIORITY_AFTER)
		{
			put_after[Config->module_names[j]] = Config->module_names[modules[j]->Prioritize() >> 8];
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

void InspIRCd::DoOneIteration(bool process_module_sockets)
{
	int activefds[MAX_DESCRIPTORS];
	int incomingSockfd;
	int in_port;
	userrec* cu = NULL;
	InspSocket* s = NULL;
	InspSocket* s_del = NULL;
	unsigned int numberactive;
	insp_sockaddr sock_us;     // our port number
	socklen_t uslen;	 // length of our port number

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
	if (((TIME % 5) == 0) && (!expire_run))
	{
		expire_lines();
		if (process_module_sockets)
		{
			/* Fix by brain - the addition of DoOneIteration means that this
			 * can end up getting called recursively in the following pattern:
			 *
			 * m_spanningtree DoPingChecks
			 * (server pings out and is squit)
			 * (squit causes call to DoOneIteration)
			 * DoOneIteration enters here
			 * calls DoBackground timer
			 * enters m_spanningtree DoPingChecks... see step 1.
			 *
			 * This should do the job and fix the bug.
			 */
			FOREACH_MOD(I_OnBackgroundTimer,OnBackgroundTimer(TIME));
		}
		TickMissedTimers(TIME);
		expire_run = true;
		return;
	}   
	else if ((TIME % 5) == 1)
	{
		expire_run = false;
	}

	if (iterations++ == 15)
	{
		iterations = 0;
		DoBackgroundUserStuff(TIME);
	}
 
	/* Once a second, do the background processing */
	if (TIME != OLDTIME)
	{
		if (TIME < OLDTIME)
			WriteOpers("*** \002EH?!\002 -- Time is flowing BACKWARDS in this dimension! Clock drifted backwards %d secs.",abs(OLDTIME-TIME));
		if ((TIME % 3600) == 0)
		{
			MaintainWhoWas(TIME);
		}
	}

	/* Process timeouts on module sockets each time around
	 * the loop. There shouldnt be many module sockets, at
	 * most, 20 or so, so this won't be much of a performance
	 * hit at all.   
	 */ 
	if (process_module_sockets)
		DoSocketTimeouts(TIME);  
	 
	TickTimers(TIME);
	 
	/* Call the socket engine to wait on the active
	 * file descriptors. The socket engine has everything's
	 * descriptors in its list... dns, modules, users,
	 * servers... so its nice and easy, just one call.
	 */
	if (!(numberactive = SE->Wait(activefds)))
		return;

	/**
	 * Now process each of the fd's. For users, we have a fast
	 * lookup table which can find a user by file descriptor, so
	 * processing them by fd isnt expensive. If we have a lot of
	 * listening ports or module sockets though, things could get
	 * ugly.
	 */
	log(DEBUG,"There are %d fd's to process.",numberactive);

	for (unsigned int activefd = 0; activefd < numberactive; activefd++)
	{
		int socket_type = SE->GetType(activefds[activefd]);
		switch (socket_type)
		{
			case X_ESTAB_CLIENT:

				log(DEBUG,"Type: X_ESTAB_CLIENT: fd=%d",activefds[activefd]);
				cu = fd_ref_table[activefds[activefd]];
				if (cu)
					ProcessUser(cu);  
	
			break;
	
			case X_ESTAB_MODULE:

				log(DEBUG,"Type: X_ESTAB_MODULE: fd=%d",activefds[activefd]);

				if (!process_module_sockets)
					break;

				/* Process module-owned sockets.
				 * Modules are encouraged to inherit their sockets from
				 * InspSocket so we can process them neatly like this.
				 */
				s = socket_ref[activefds[activefd]]; 
	      
				if ((s) && (!s->Poll()))
				{
					log(DEBUG,"Socket poll returned false, close and bail");
					SE->DelFd(s->GetFd());
					socket_ref[activefds[activefd]] = NULL;
					for (std::vector<InspSocket*>::iterator a = module_sockets.begin(); a < module_sockets.end(); a++)
					{
						s_del = *a;
						if ((s_del) && (s_del->GetFd() == activefds[activefd]))
						{
							module_sockets.erase(a);
							break;
						}
					}
					s->Close();
					DELETE(s);
				}
				else if (!s)
				{
					log(DEBUG,"WTF, X_ESTAB_MODULE for nonexistent InspSocket, removed!");
					SE->DelFd(s->GetFd());
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
				log(DEBUG,"Type: X_ESTAB_DNS: fd=%d",activefds[activefd]);
				dns_poll(activefds[activefd]);
#endif
			break;

			case X_LISTEN:

				log(DEBUG,"Type: X_LISTEN_MODULE: fd=%d",activefds[activefd]);

				/* It's a listener */
				uslen = sizeof(sock_us);
				length = sizeof(client);
				incomingSockfd = accept (activefds[activefd],(struct sockaddr*)&client,&length);
	
				if ((incomingSockfd > -1) && (!getsockname(incomingSockfd,(sockaddr*)&sock_us,&uslen)))
				{
					in_port = ntohs(sock_us.sin_port);
					log(DEBUG,"Accepted socket %d",incomingSockfd);
					/* Years and years ago, we used to resolve here
					 * using gethostbyaddr(). That is sucky and we
					 * don't do that any more...
					 */
					NonBlocking(incomingSockfd);
					if (Config->GetIOHook(in_port))
					{
						try
						{
							Config->GetIOHook(in_port)->OnRawSocketAccept(incomingSockfd, inet_ntoa(client.sin_addr), in_port);
						}
						catch (ModuleException& modexcept)
						{
							log(DEBUG,"Module exception cought: %s",modexcept.GetReason());
						}
					}
					stats->statsAccept++;
					AddClient(incomingSockfd, in_port, false, client.sin_addr);
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
				log(DEBUG,"Type: X_WHAT_THE_FUCK_BBQ: fd=%d",activefds[activefd]);
				SE->DelFd(activefds[activefd]);
			break;
		}
	}
	yield_depth--;
}

int InspIRCd::Run()
{
	/* Until THIS point, ServerInstance == NULL */
	
	LoadAllModules(this);

	/* Just in case no modules were loaded - fix for bug #101 */
	this->BuildISupport();

	printf("\nInspIRCd is now running!\n");
	
	if (!Config->nofork)
	{
		fclose(stdout);
		fclose(stderr);
		fclose(stdin);
	}

	/* Add the listening sockets used for client inbound connections
	 * to the socket engine
	 */
	for (int count = 0; count < stats->BoundPortCount; count++)
		SE->AddFd(Config->openSockfd[count],true,X_LISTEN);

	this->WritePID(Config->PID);

	/* main loop, this never returns */
	expire_run = false;
	yield_depth = 0;
	iterations = 0;

	while (true)
	{
		DoOneIteration(true);
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
	try
	{
		ServerInstance = new InspIRCd(argc, argv);
		ServerInstance->Run();
		DELETE(ServerInstance);
	}
	catch (std::bad_alloc)
	{
		log(DEFAULT,"You are out of memory! (got exception std::bad_alloc!)");
		send_error("**** OUT OF MEMORY **** We're gonna need a bigger boat!");
		printf("Out of memory! (got exception std::bad_alloc!");
	}
	return 0;
}
