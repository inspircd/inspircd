/* ---------------------------------------------------------------------
 * 
 *	      +------------------------------------+
 *	      | Inspire Internet Relay Chat Daemon |
 *	      +------------------------------------+
 *
 *	 InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *			     E-mail:
 *		      <brain@chatspike.net>
 *		      <Craig@chatspike.net>
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

#include "inspircd.h"
#include "configreader.h"
#include <signal.h>
#include <exception>
#include <fstream>
#include "modules.h"
#include "mode.h"
#include "xline.h"
#include "socketengine.h"
#include "inspircd_se_config.h"
#include "socket.h"
#include "typedefs.h"
#include "command_parse.h"
#include <dlfcn.h>

using irc::sockets::NonBlocking;
using irc::sockets::Blocking;
using irc::sockets::insp_ntoa;
using irc::sockets::insp_inaddr;
using irc::sockets::insp_sockaddr;

InspIRCd* SI = NULL;

void InspIRCd::AddServerName(const std::string &servername)
{
	this->Log(DEBUG,"Adding server name: %s",servername.c_str());
	
	if(find(servernames.begin(), servernames.end(), servername) == servernames.end())
		servernames.push_back(servername); /* Wasn't already there. */
}

const char* InspIRCd::FindServerNamePtr(const std::string &servername)
{
	servernamelist::iterator iter = find(servernames.begin(), servernames.end(), servername);
	
	if(iter == servernames.end())
	{		
		AddServerName(servername);
		iter = --servernames.end();
	}

	return iter->c_str();
}

bool InspIRCd::FindServerName(const std::string &servername)
{
	return (find(servernames.begin(), servernames.end(), servername) != servernames.end());
}

void InspIRCd::Exit(int status)
{
	exit (status);
}

void InspIRCd::Start()
{
	printf("\033[1;32mInspire Internet Relay Chat Server, compiled %s at %s\n",__DATE__,__TIME__);
	printf("(C) ChatSpike Development team.\033[0m\n\n");
	printf("Developers:\t\t\033[1;32mBrain, FrostyCoolSlug, w00t, Om, Special, pippijn, jamie\033[0m\n");
	printf("Others:\t\t\t\033[1;32mSee /INFO Output\033[0m\n");
	printf("Name concept:\t\t\033[1;32mLord_Zathras\033[0m\n\n");
}

void InspIRCd::Rehash(int status)
{
	SI->WriteOpers("Rehashing config file %s due to SIGHUP",ServerConfig::CleanFilename(CONFIG_FILE));
	fclose(SI->Config->log_file);
	SI->OpenLog(NULL,0);
	SI->Config->Read(false,NULL);
	FOREACH_MOD_I(SI,I_OnRehash,OnRehash(""));
}

void InspIRCd::SetSignals()
{
	signal(SIGALRM, SIG_IGN);
	signal(SIGHUP, InspIRCd::Rehash);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, InspIRCd::Exit);
	signal(SIGCHLD, SIG_IGN);
}

bool InspIRCd::DaemonSeed()
{
	int childpid;
	if ((childpid = fork ()) < 0)
		return false;
	else if (childpid > 0)
	{
		/* We wait here for the child process to kill us,
		 * so that the shell prompt doesnt come back over
		 * the output.
		 * Sending a kill with a signal of 0 just checks
		 * if the child pid is still around. If theyre not,
		 * they threw an error and we should give up.
		 */
		while (kill(childpid, 0) != -1)
			sleep(1);
		exit(ERROR);
	}
	setsid ();
	umask (007);
	printf("InspIRCd Process ID: \033[1;32m%lu\033[0m\n",(unsigned long)getpid());

	rlimit rl;
	if (getrlimit(RLIMIT_CORE, &rl) == -1)
	{
		this->Log(DEFAULT,"Failed to getrlimit()!");
		return false;
	}
	else
	{
		rl.rlim_cur = rl.rlim_max;
		if (setrlimit(RLIMIT_CORE, &rl) == -1)
			this->Log(DEFAULT,"setrlimit() failed, cannot increase coredump size.");
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
		this->Log(DEFAULT,"Failed to write PID-file '%s', exiting.",filename.c_str());
		Exit(0);
	}
}

std::string InspIRCd::GetRevision()
{
	return REVISION;
}

InspIRCd::InspIRCd(int argc, char** argv)
	: ModCount(-1), duration_m(60), duration_h(60*60), duration_d(60*60*24), duration_w(60*60*24*7), duration_y(60*60*24*365)
{
	modules.resize(255);
	factory.resize(255);
	
	this->Config = new ServerConfig(this);
	this->SNO = new SnomaskManager(this);
	this->Start();
	this->TIME = this->OLDTIME = this->startup_time = time(NULL);
	srand(this->TIME);
	this->Log(DEBUG,"*** InspIRCd starting up!");
	if (!ServerConfig::FileExists(CONFIG_FILE))
	{
		printf("ERROR: Cannot open config file: %s\nExiting...\n",CONFIG_FILE);
		this->Log(DEFAULT,"main: no config");
		printf("ERROR: Your config file is missing, this IRCd will self destruct in 10 seconds!\n");
		Exit(ERROR);
	}
	*this->LogFileName = 0;
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
			else if (!strcmp(argv[i],"-logfile"))
			{
				if (argc > i+1)
				{
					strlcpy(LogFileName,argv[i+1],MAXBUF);
					printf("LOG: Setting logfile to %s\n",LogFileName);
				}
				else
				{
					printf("ERROR: The -logfile parameter must be followed by a log file name and path.\n");
					Exit(ERROR);
				}
				i++;
			}
			else
			{
				printf("Usage: %s [-nofork] [-nolog] [-debug] [-wait] [-logfile <filename>]\n",argv[0]);
				Exit(ERROR);
			}
		}
	}

	strlcpy(Config->MyExecutable,argv[0],MAXBUF);

	this->OpenLog(argv, argc);
	this->stats = new serverstats();
	this->Parser = new CommandParser(this);
	this->Timers = new TimerManager();
	this->XLines = new XLineManager(this);
	Config->ClearStack();
	Config->Read(true, NULL);
	this->CheckRoot();
	this->Modes = new ModeParser(this);
	this->AddServerName(Config->ServerName);	
	CheckDie();
	InitializeDisabledCommands(Config->DisabledCommands, this);
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
	SocketEngineFactory* SEF = new SocketEngineFactory();
	SE = SEF->Create(this);
	delete SEF;

	this->Res = new DNS(this);

	this->LoadAllModules();
	/* Just in case no modules were loaded - fix for bug #101 */
	this->BuildISupport();

	if (!stats->BoundPortCount)
	{
		printf("\nERROR: I couldn't bind any ports! Are you sure you didn't start InspIRCd twice?\n");
		Exit(ERROR);
	}

	/* Add the listening sockets used for client inbound connections
	 * to the socket engine
	 */
	this->Log(DEBUG,"%d listeners",stats->BoundPortCount);
	for (unsigned long count = 0; count < stats->BoundPortCount; count++)
	{
		this->Log(DEBUG,"Add listener: %d",Config->openSockfd[count]->GetFd());
		if (!SE->AddFd(Config->openSockfd[count]))
		{
			printf("\nEH? Could not add listener to socketengine. You screwed up, aborting.\n");
			Exit(ERROR);
		}
	}

	if (!Config->nofork)
	{
		if (kill(getppid(), SIGTERM) == -1)
			printf("Error killing parent process: %s\n",strerror(errno));
		fclose(stdin);
		fclose(stderr);
		fclose(stdout);
	}

	printf("\nInspIRCd is now running!\n");

	this->WritePID(Config->PID);
}

std::string InspIRCd::GetVersionString()
{
	char versiondata[MAXBUF];
	char dnsengine[] = "singlethread-object";
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
			delete *t;
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
	for (ModuleList::iterator m = modules.begin(); m!= modules.end(); m++)
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
			// found an instance, swap it with the item at the end
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
		this->Log(DEBUG,"Move of %s to slot failed!",modulename.c_str());
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
	MoveTo(modulename,this->GetModuleCount());
}

void InspIRCd::BuildISupport()
{
	// the neatest way to construct the initial 005 numeric, considering the number of configure constants to go in it...
	std::stringstream v;
	v << "WALLCHOPS WALLVOICES MODES=" << MAXMODES << " CHANTYPES=# PREFIX=" << this->Modes->BuildPrefixes() << " MAP MAXCHANNELS=" << MAXCHANS << " MAXBANS=60 VBANLIST NICKLEN=" << NICKMAX-1;
	v << " CASEMAPPING=rfc1459 STATUSMSG=@%+ CHARSET=ascii TOPICLEN=" << MAXTOPIC << " KICKLEN=" << MAXKICK << " MAXTARGETS=" << Config->MaxTargets << " AWAYLEN=";
	v << MAXAWAY << " CHANMODES=" << this->Modes->ChanModes() << " FNC NETWORK=" << Config->Network << " MAXPARA=32";
	Config->data005 = v.str();
	FOREACH_MOD_I(this,I_On005Numeric,On005Numeric(Config->data005));
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
				this->Log(DEFAULT,"Failed to unload STATIC module %s",filename);
				snprintf(MODERR,MAXBUF,"Module not unloadable (marked static)");
				return false;
			}
			/* Give the module a chance to tidy out all its metadata */
			for (chan_hash::iterator c = this->chanlist.begin(); c != this->chanlist.end(); c++)
			{
				modules[j]->OnCleanup(TYPE_CHANNEL,c->second);
			}
			for (user_hash::iterator u = this->clientlist.begin(); u != this->clientlist.end(); u++)
			{
				modules[j]->OnCleanup(TYPE_USER,u->second);
			}

			FOREACH_MOD_I(this,I_OnUnloadModule,OnUnloadModule(modules[j],Config->module_names[j]));

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
			this->Log(DEBUG,"Removing dependent commands...");
			Parser->RemoveCommands(filename);
			this->Log(DEBUG,"Deleting module...");
			this->EraseModule(j);
			this->Log(DEBUG,"Erasing module entry...");
			this->EraseFactory(j);
			this->Log(DEFAULT,"Module %s unloaded",filename);
			this->ModCount--;
			BuildISupport();
			return true;
		}
	}
	this->Log(DEFAULT,"Module %s is not loaded, cannot unload it!",filename);
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
	if (!ServerConfig::DirValid(modfile))
	{
		this->Log(DEFAULT,"Module %s is not within the modules directory.",modfile);
		snprintf(MODERR,MAXBUF,"Module %s is not within the modules directory.",modfile);
		return false;
	}
#endif
#endif
	this->Log(DEBUG,"Loading module: %s",modfile);
#ifndef STATIC_LINK
	if (ServerConfig::FileExists(modfile))
	{
#endif
		for (unsigned int j = 0; j < Config->module_names.size(); j++)
		{
			if (Config->module_names[j] == filename_str)
			{
				this->Log(DEFAULT,"Module %s is already loaded, cannot load a module twice!",modfile);
				snprintf(MODERR,MAXBUF,"Module already loaded");
				return false;
			}
		}
		try
		{
			ircd_module* a = new ircd_module(this, modfile);
			factory[this->ModCount+1] = a;
			if (factory[this->ModCount+1]->LastError())
			{
				this->Log(DEFAULT,"Unable to load %s: %s",modfile,factory[this->ModCount+1]->LastError());
				snprintf(MODERR,MAXBUF,"Loader/Linker error: %s",factory[this->ModCount+1]->LastError());
				return false;
			}
			if ((long)factory[this->ModCount+1]->factory != -1)
			{
				Module* m = factory[this->ModCount+1]->factory->CreateModule(this);

				Version v = m->GetVersion();

				if (v.API != API_VERSION)
				{
					delete m;
					delete a;
					this->Log(DEFAULT,"Unable to load %s: Incorrect module API version: %d (our version: %d)",modfile,v.API,API_VERSION);
					snprintf(MODERR,MAXBUF,"Loader/Linker error: Incorrect module API version: %d (our version: %d)",v.API,API_VERSION);
					return false;
				}

				modules[this->ModCount+1] = m;
				/* save the module and the module's classfactory, if
				 * this isnt done, random crashes can occur :/ */
				Config->module_names.push_back(filename);

				char* x = &Config->implement_lists[this->ModCount+1][0];
				for(int t = 0; t < 255; t++)
					x[t] = 0;

				modules[this->ModCount+1]->Implements(x);

				for(int t = 0; t < 255; t++)
					Config->global_implementation[t] += Config->implement_lists[this->ModCount+1][t];
			}
			else
			{
       				this->Log(DEFAULT,"Unable to load %s",modfile);
				snprintf(MODERR,MAXBUF,"Factory function failed: Probably missing init_module() entrypoint.");
				return false;
			}
		}
		catch (ModuleException& modexcept)
		{
			this->Log(DEFAULT,"Unable to load %s: ",modfile,modexcept.GetReason());
			snprintf(MODERR,MAXBUF,"Factory function threw an exception: %s",modexcept.GetReason());
			return false;
		}
#ifndef STATIC_LINK
	}
	else
	{
		this->Log(DEFAULT,"InspIRCd: startup: Module Not Found %s",modfile);
		snprintf(MODERR,MAXBUF,"Module file could not be found");
		return false;
	}
#endif
	this->ModCount++;
	FOREACH_MOD_I(this,I_OnLoadModule,OnLoadModule(modules[this->ModCount],filename_str));
	// now work out which modules, if any, want to move to the back of the queue,
	// and if they do, move them there.
	std::vector<std::string> put_to_back;
	std::vector<std::string> put_to_front;
	std::map<std::string,std::string> put_before;
	std::map<std::string,std::string> put_after;
	for (unsigned int j = 0; j < Config->module_names.size(); j++)
	{
		if (modules[j]->Prioritize() == PRIORITY_LAST)
			put_to_back.push_back(Config->module_names[j]);
		else if (modules[j]->Prioritize() == PRIORITY_FIRST)
			put_to_front.push_back(Config->module_names[j]);
		else if ((modules[j]->Prioritize() & 0xFF) == PRIORITY_BEFORE)
			put_before[Config->module_names[j]] = Config->module_names[modules[j]->Prioritize() >> 8];
		else if ((modules[j]->Prioritize() & 0xFF) == PRIORITY_AFTER)
			put_after[Config->module_names[j]] = Config->module_names[modules[j]->Prioritize() >> 8];
	}
	for (unsigned int j = 0; j < put_to_back.size(); j++)
		MoveToLast(put_to_back[j]);
	for (unsigned int j = 0; j < put_to_front.size(); j++)
		MoveToFirst(put_to_front[j]);
	for (std::map<std::string,std::string>::iterator j = put_before.begin(); j != put_before.end(); j++)
		MoveBefore(j->first,j->second);
	for (std::map<std::string,std::string>::iterator j = put_after.begin(); j != put_after.end(); j++)
		MoveAfter(j->first,j->second);
	BuildISupport();
	return true;
}

void InspIRCd::DoOneIteration(bool process_module_sockets)
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
	if (TIME != OLDTIME)
	{
		if (TIME < OLDTIME)
			WriteOpers("*** \002EH?!\002 -- Time is flowing BACKWARDS in this dimension! Clock drifted backwards %d secs.",abs(OLDTIME-TIME));
		if ((TIME % 3600) == 0)
		{
			irc::whowas::MaintainWhoWas(this, TIME);
		}
		Timers->TickTimers(TIME);
		this->DoBackgroundUserStuff(TIME);

		if ((TIME % 5) == 0)
		{
			XLines->expire_lines();
			FOREACH_MOD_I(this,I_OnBackgroundTimer,OnBackgroundTimer(TIME));
			Timers->TickMissedTimers(TIME);
		}
	}

	/* Call the socket engine to wait on the active
	 * file descriptors. The socket engine has everything's
	 * descriptors in its list... dns, modules, users,
	 * servers... so its nice and easy, just one call.
	 * This will cause any read or write events to be 
	 * dispatched to their handlers.
	 */
	SE->DispatchEvents();
}

bool InspIRCd::IsIdent(const char* n)
{
	if (!n || !*n)
		return false;

	for (char* i = (char*)n; *i; i++)
	{
		if ((*i >= 'A') && (*i <= '}'))
		{
			continue;
		}
		if (((*i >= '0') && (*i <= '9')) || (*i == '-') || (*i == '.'))
		{
			continue;
		}
		return false;
	}
	return true;
}


int InspIRCd::Run()
{
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
	SI = new InspIRCd(argc, argv);
	SI->Run();
	delete SI;
	return 0;
}

/* this returns true when all modules are satisfied that the user should be allowed onto the irc server
 * (until this returns true, a user will block in the waiting state, waiting to connect up to the
 * registration timeout maximum seconds)
 */
bool InspIRCd::AllModulesReportReady(userrec* user)
{
	if (!Config->global_implementation[I_OnCheckReady])
		return true;

	for (int i = 0; i <= this->GetModuleCount(); i++)
	{
		if (Config->implement_lists[i][I_OnCheckReady])
		{
			int res = modules[i]->OnCheckReady(user);
			if (!res)
				return false;
		}
	}
	return true;
}

int InspIRCd::GetModuleCount()
{
	return this->ModCount;
}

time_t InspIRCd::Time()
{
	return TIME;
}

bool FileLogger::Readable()
{
	return false;
}

void FileLogger::HandleEvent(EventType et)
{
	this->WriteLogLine("");
	ServerInstance->SE->DelFd(this);
}

void FileLogger::WriteLogLine(const std::string &line)
{
	if (line.length())
		buffer.append(line);

	if (log)
	{
		int written = fprintf(log,"%s",buffer.c_str());
		if ((written >= 0) && (written < (int)buffer.length()))
		{
			buffer.erase(0, buffer.length());
			ServerInstance->SE->AddFd(this);
		}
		else if (written == -1)
		{
			if (errno == EAGAIN)
				ServerInstance->SE->AddFd(this);
		}
		else
		{
			/* Wrote the whole buffer, and no need for write callback */
			buffer = "";
		}
	}
	if (writeops++ % 20)
	{
		fflush(log);
	}
}

void FileLogger::Close()
{
	if (log)
	{
		int flags = fcntl(fileno(log), F_GETFL, 0);
		fcntl(fileno(log), F_SETFL, flags ^ O_NONBLOCK);
		if (buffer.size())
			fprintf(log,"%s",buffer.c_str());
		fflush(log);
		fclose(log);
	}
	buffer = "";
	ServerInstance->SE->DelFd(this);
}

FileLogger::FileLogger(InspIRCd* Instance, FILE* logfile) : ServerInstance(Instance), log(logfile), writeops(0)
{
	irc::sockets::NonBlocking(fileno(log));
	this->SetFd(fileno(log));
	buffer = "";
}

FileLogger::~FileLogger()
{
	this->Close();
}

