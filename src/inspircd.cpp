/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "configreader.h"
#include <signal.h>

#ifndef WIN32
#include <dirent.h>
#include <unistd.h>
#include <sys/resource.h>
#include <dlfcn.h>
#include <getopt.h>

/* Some systems don't define RUSAGE_SELF. This should fix them. */
#ifndef RUSAGE_SELF
	#define RUSAGE_SELF 0
#endif

#endif

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
#include "exitcodes.h"

#ifdef WIN32

/* This MUST remain static and delcared outside the class, so that WriteProcessMemory can reference it properly */
static DWORD owner_processid = 0;

DWORD WindowsForkStart(InspIRCd * Instance)
{
	/* Windows implementation of fork() :P */

	char module[MAX_PATH];
	if(!GetModuleFileName(NULL, module, MAX_PATH))
	{
		printf("GetModuleFileName() failed.\n");
		return false;
	}

	STARTUPINFO startupinfo;
	PROCESS_INFORMATION procinfo;
	ZeroMemory(&startupinfo, sizeof(STARTUPINFO));
	ZeroMemory(&procinfo, sizeof(PROCESS_INFORMATION));

	// Fill in the startup info struct
	GetStartupInfo(&startupinfo);

	/* Default creation flags create the processes suspended */
	DWORD startupflags = CREATE_SUSPENDED;

	/* On windows 2003/XP and above, we can use the value
	 * CREATE_PRESERVE_CODE_AUTHZ_LEVEL which gives more access
	 * to the process which we may require on these operating systems.
	 */
	OSVERSIONINFO vi;
	vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&vi);
	if ((vi.dwMajorVersion >= 5) && (vi.dwMinorVersion > 0))
		startupflags |= CREATE_PRESERVE_CODE_AUTHZ_LEVEL;

	// Launch our "forked" process.
	BOOL bSuccess = CreateProcess ( module,	// Module (exe) filename
		strdup(GetCommandLine()),	// Command line (exe plus parameters from the OS)
						// NOTE: We cannot return the direct value of the
						// GetCommandLine function here, as the pointer is
						// passed straight to the child process, and will be
						// invalid once we exit as it goes out of context.
						// strdup() seems ok, though.
		0,				// PROCESS_SECURITY_ATTRIBUTES
		0,				// THREAD_SECURITY_ATTRIBUTES
		TRUE,				// We went to inherit handles.
		startupflags,			// Allow us full access to the process and suspend it.
		0,				// ENVIRONMENT
		0,				// CURRENT_DIRECTORY
		&startupinfo,			// startup info
		&procinfo);			// process info

	if(!bSuccess)
	{
		printf("CreateProcess() error: %s\n", dlerror());
		return false;
	}

	// Set the owner process id in the target process.
	SIZE_T written = 0;
	DWORD pid = GetCurrentProcessId();
	if(!WriteProcessMemory(procinfo.hProcess, &owner_processid, &pid, sizeof(DWORD), &written) || written != sizeof(DWORD))
	{
		printf("WriteProcessMemory() failed: %s\n", dlerror());
		return false;
	}

	// Resume the other thread (let it start)
	ResumeThread(procinfo.hThread);

	// Wait for the new process to kill us. If there is some error, the new process will end and we will end up at the next line.
	WaitForSingleObject(procinfo.hProcess, INFINITE);

	// If we hit this it means startup failed, default to 14 if this fails.
	DWORD ExitCode = 14;
	GetExitCodeProcess(procinfo.hProcess, &ExitCode);
	CloseHandle(procinfo.hThread);
	CloseHandle(procinfo.hProcess);
	return ExitCode;
}

void WindowsForkKillOwner(InspIRCd * Instance)
{
	HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, owner_processid);
	if(!hProcess || !owner_processid)
	{
		printf("Could not open process id %u: %s.\n", owner_processid, dlerror());
		Instance->Exit(14);
	}

	// die die die
	if(!TerminateProcess(hProcess, 0))
	{
		printf("Could not TerminateProcess(): %s\n", dlerror());
		Instance->Exit(14);
	}

	CloseHandle(hProcess);
}

#endif

using irc::sockets::NonBlocking;
using irc::sockets::Blocking;
using irc::sockets::insp_ntoa;
using irc::sockets::insp_inaddr;
using irc::sockets::insp_sockaddr;

InspIRCd* SI = NULL;

/* Burlex: Moved from exitcodes.h -- due to duplicate symbols */
const char* ExitCodes[] =
{
		"No error", /* 0 */
		"DIE command", /* 1 */
		"execv() failed", /* 2 */
		"Internal error", /* 3 */
		"Config file error", /* 4 */
		"Logfile error", /* 5 */
		"POSIX fork failed", /* 6 */
		"Bad commandline parameters", /* 7 */
		"No ports could be bound", /* 8 */
		"Can't write PID file", /* 9 */
		"SocketEngine could not initialize", /* 10 */
		"Refusing to start up as root", /* 11 */
		"Found a <die> tag!", /* 12 */
		"Couldn't load module on startup", /* 13 */
		"Could not create windows forked process", /* 14 */
		"Received SIGTERM", /* 15 */
};

void InspIRCd::AddServerName(const std::string &servername)
{
	servernamelist::iterator itr = servernames.begin();
	for(; itr != servernames.end(); ++itr)
		if(**itr == servername)
			return;

	string * ns = new string(servername);
	servernames.push_back(ns);
}

const char* InspIRCd::FindServerNamePtr(const std::string &servername)
{
	servernamelist::iterator itr = servernames.begin();
	for(; itr != servernames.end(); ++itr)
		if(**itr == servername)
			return (*itr)->c_str();

	servernames.push_back(new string(servername));
	itr = --servernames.end();
	return (*itr)->c_str();
}

bool InspIRCd::FindServerName(const std::string &servername)
{
	servernamelist::iterator itr = servernames.begin();
	for(; itr != servernames.end(); ++itr)
		if(**itr == servername)
			return true;
	return false;
}

void InspIRCd::Exit(int status)
{
#ifdef WINDOWS
	CloseIPC();
#endif
	if (SI)
	{
		SI->SendError("Exiting with status " + ConvToStr(status) + " (" + std::string(ExitCodes[status]) + ")");
		SI->Cleanup();
	}
	exit (status);
}

void InspIRCd::Cleanup()
{
	std::vector<std::string> mymodnames;
	int MyModCount = this->GetModuleCount();

	for (unsigned int i = 0; i < Config->ports.size(); i++)
	{
		/* This calls the constructor and closes the listening socket */
		delete Config->ports[i];
	}

	Config->ports.clear();

	/* Close all client sockets, or the new process inherits them */
	for (std::vector<userrec*>::const_iterator i = this->local_users.begin(); i != this->local_users.end(); i++)
	{
		(*i)->SetWriteError("Server shutdown");
		(*i)->CloseSocket();
	}

	/* We do this more than once, so that any service providers get a
	 * chance to be unhooked by the modules using them, but then get
	 * a chance to be removed themsleves.
	 */
	for (int tries = 0; tries < 3; tries++)
	{
		MyModCount = this->GetModuleCount();
		mymodnames.clear();

		/* Unload all modules, so they get a chance to clean up their listeners */
		for (int j = 0; j <= MyModCount; j++)
			mymodnames.push_back(Config->module_names[j]);

		for (int k = 0; k <= MyModCount; k++)
			this->UnloadModule(mymodnames[k].c_str());
	}

	/* Close logging */
	this->Logger->Close();

	/* Cleanup Server Names */
	for(servernamelist::iterator itr = servernames.begin(); itr != servernames.end(); ++itr)
		delete (*itr);

#ifdef WINDOWS
	/* WSACleanup */
	WSACleanup();
#endif
}

void InspIRCd::Restart(const std::string &reason)
{
	/* SendError flushes each client's queue,
	 * regardless of writeability state
	 */
	this->SendError(reason);

	this->Cleanup();

	/* Figure out our filename (if theyve renamed it, we're boned) */
	std::string me;

#ifdef WINDOWS
	char module[MAX_PATH];
	if (GetModuleFileName(NULL, module, MAX_PATH))
		me = module;
#else
	me = Config->MyDir + "/inspircd";
#endif

	if (execv(me.c_str(), Config->argv) == -1)
	{
		/* Will raise a SIGABRT if not trapped */
		throw CoreException(std::string("Failed to execv()! error: ") + strerror(errno));
	}
}

void InspIRCd::Start()
{
	printf_c("\033[1;32mInspire Internet Relay Chat Server, compiled %s at %s\n",__DATE__,__TIME__);
	printf_c("(C) InspIRCd Development Team.\033[0m\n\n");
	printf_c("Developers:\t\t\033[1;32mBrain, FrostyCoolSlug, w00t, Om, Special, pippijn, peavey, Burlex\033[0m\n");
	printf_c("Others:\t\t\t\033[1;32mSee /INFO Output\033[0m\n");
}

void InspIRCd::Rehash(int status)
{
	SI->WriteOpers("*** Rehashing config file %s due to SIGHUP",ServerConfig::CleanFilename(SI->ConfigFileName));
	SI->CloseLog();
	SI->OpenLog(SI->Config->argv, SI->Config->argc);
	SI->RehashUsersAndChans();
	FOREACH_MOD_I(SI, I_OnGarbageCollect, OnGarbageCollect());
	SI->Config->Read(false,NULL);
	SI->ResetMaxBans();
	SI->Res->Rehash();
	FOREACH_MOD_I(SI,I_OnRehash,OnRehash(NULL,""));
	SI->BuildISupport();
}

void InspIRCd::ResetMaxBans()
{
	for (chan_hash::const_iterator i = chanlist->begin(); i != chanlist->end(); i++)
		i->second->ResetMaxBans();
}


/** Because hash_map doesnt free its buckets when we delete items (this is a 'feature')
 * we must occasionally rehash the hash (yes really).
 * We do this by copying the entries from the old hash to a new hash, causing all
 * empty buckets to be weeded out of the hash. We dont do this on a timer, as its
 * very expensive, so instead we do it when the user types /REHASH and expects a
 * short delay anyway.
 */
void InspIRCd::RehashUsersAndChans()
{
	user_hash* old_users = this->clientlist;
	chan_hash* old_chans = this->chanlist;

	this->clientlist = new user_hash();
	this->chanlist = new chan_hash();

	for (user_hash::const_iterator n = old_users->begin(); n != old_users->end(); n++)
		this->clientlist->insert(*n);

	delete old_users;

	for (chan_hash::const_iterator n = old_chans->begin(); n != old_chans->end(); n++)
		this->chanlist->insert(*n);

	delete old_chans;
}

void InspIRCd::CloseLog()
{
	this->Logger->Close();
}

void InspIRCd::SetSignals()
{
#ifndef WIN32
	signal(SIGALRM, SIG_IGN);
	signal(SIGHUP, InspIRCd::Rehash);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
#endif
	signal(SIGTERM, InspIRCd::Exit);
}

void InspIRCd::QuickExit(int status)
{
	exit(0);
}

bool InspIRCd::DaemonSeed()
{
#ifdef WINDOWS
	printf_c("InspIRCd Process ID: \033[1;32m%lu\033[0m\n", GetCurrentProcessId());
	return true;
#else
	signal(SIGTERM, InspIRCd::QuickExit);

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
		exit(0);
	}
	setsid ();
	umask (007);
	printf("InspIRCd Process ID: \033[1;32m%lu\033[0m\n",(unsigned long)getpid());

	signal(SIGTERM, InspIRCd::Exit);

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
#endif
}

void InspIRCd::WritePID(const std::string &filename)
{
	std::string fname = (filename.empty() ? "inspircd.pid" : filename);
	if (*(fname.begin()) != '/')
	{
		std::string::size_type pos;
		std::string confpath = this->ConfigFileName;
		if ((pos = confpath.rfind("/")) != std::string::npos)
		{
			/* Leaves us with just the path */
			fname = confpath.substr(0, pos) + std::string("/") + fname;
		}
	}
	std::ofstream outfile(fname.c_str());
	if (outfile.is_open())
	{
		outfile << getpid();
		outfile.close();
	}
	else
	{
		printf("Failed to write PID-file '%s', exiting.\n",fname.c_str());
		this->Log(DEFAULT,"Failed to write PID-file '%s', exiting.",fname.c_str());
		Exit(EXIT_STATUS_PID);
	}
}

std::string InspIRCd::GetRevision()
{
	return REVISION;
}

InspIRCd::InspIRCd(int argc, char** argv)
	: ModCount(-1), GlobalCulls(this)
{
	int found_ports = 0;
	FailedPortList pl;
	int do_version = 0, do_nofork = 0, do_debug = 0, do_nolog = 0, do_root = 0;    /* flag variables */
	char c = 0;

	modules.resize(255);
	factory.resize(255);
	memset(&server, 0, sizeof(server));
	memset(&client, 0, sizeof(client));

	this->unregistered_count = 0;

	this->clientlist = new user_hash();
	this->chanlist = new chan_hash();

	this->Config = new ServerConfig(this);

	this->Config->argv = argv;
	this->Config->argc = argc;

	if (chdir(Config->GetFullProgDir().c_str()))
	{
		printf("Unable to change to my directory: %s\nAborted.", strerror(errno));
		exit(0);
	}

	this->Config->opertypes.clear();
	this->Config->operclass.clear();
	this->SNO = new SnomaskManager(this);
	this->TIME = this->OLDTIME = this->startup_time = time(NULL);
	this->time_delta = 0;
	this->next_call = this->TIME + 3;
	srand(this->TIME);

	*this->LogFileName = 0;
	strlcpy(this->ConfigFileName, CONFIG_FILE, MAXBUF);

	struct option longopts[] =
	{
		{ "nofork",	no_argument,		&do_nofork,	1	},
		{ "logfile",	required_argument,	NULL,		'f'	},
		{ "config",	required_argument,	NULL,		'c'	},
		{ "debug",	no_argument,		&do_debug,	1	},
		{ "nolog",	no_argument,		&do_nolog,	1	},
		{ "runasroot",	no_argument,		&do_root,	1	},
		{ "version",	no_argument,		&do_version,	1	},
		{ 0, 0, 0, 0 }
	};

	while ((c = getopt_long_only(argc, argv, ":f:", longopts, NULL)) != -1)
	{
		switch (c)
		{
			case 'f':
				/* Log filename was set */
				strlcpy(LogFileName, optarg, MAXBUF);
			break;
			case 'c':
				/* Config filename was set */
				strlcpy(ConfigFileName, optarg, MAXBUF);
			break;
			case 0:
				/* getopt_long_only() set an int variable, just keep going */
			break;
			default:
				/* Unknown parameter! DANGER, INTRUDER.... err.... yeah. */
				printf("Usage: %s [--nofork] [--nolog] [--debug] [--logfile <filename>] [--runasroot] [--version] [--config <config>]\n", argv[0]);
				Exit(EXIT_STATUS_ARGV);
			break;
		}
	}

	if (do_version)
	{
		printf("\n%s r%s\n", VERSION, REVISION);
		Exit(EXIT_STATUS_NOERROR);
	}

#ifdef WIN32

	// Handle forking
	if(!do_nofork && !owner_processid)
	{
		DWORD ExitCode = WindowsForkStart(this);
		if(ExitCode)
			Exit(ExitCode);
	}

	// Set up winsock
	WSADATA wsadata;
	WSAStartup(MAKEWORD(2,0), &wsadata);

#endif
	if (!ServerConfig::FileExists(this->ConfigFileName))
	{
		printf("ERROR: Cannot open config file: %s\nExiting...\n", this->ConfigFileName);
		this->Log(DEFAULT,"Unable to open config file %s", this->ConfigFileName);
		Exit(EXIT_STATUS_CONFIG);
	}

	this->Start();

	/* Set the finished argument values */
	Config->nofork = do_nofork;
	Config->forcedebug = do_debug;
	Config->writelog = !do_nolog;

	strlcpy(Config->MyExecutable,argv[0],MAXBUF);

	this->OpenLog(argv, argc);

	this->stats = new serverstats();
	this->Timers = new TimerManager(this);
	this->Parser = new CommandParser(this);
	this->XLines = new XLineManager(this);
	Config->ClearStack();
	Config->Read(true, NULL);

	if (!do_root)
		this->CheckRoot();
	else
	{
		printf("* WARNING * WARNING * WARNING * WARNING * WARNING * \n\n");
		printf("YOU ARE RUNNING INSPIRCD AS ROOT. THIS IS UNSUPPORTED\n");
		printf("AND IF YOU ARE HACKED, CRACKED, SPINDLED OR MUTILATED\n");
		printf("OR ANYTHING ELSE UNEXPECTED HAPPENS TO YOU OR YOUR\n");
		printf("SERVER, THEN IT IS YOUR OWN FAULT. IF YOU DID NOT MEAN\n");
		printf("TO START INSPIRCD AS ROOT, HIT CTRL+C NOW AND RESTART\n");
		printf("THE PROGRAM AS A NORMAL USER. YOU HAVE BEEN WARNED!\n");
		printf("\nInspIRCd starting in 20 seconds, ctrl+c to abort...\n");
		sleep(20);
	}

	this->SetSignals();

	if (!Config->nofork)
	{
		if (!this->DaemonSeed())
		{
			printf("ERROR: could not go into daemon mode. Shutting down.\n");
			Log(DEFAULT,"ERROR: could not go into daemon mode. Shutting down.");
			Exit(EXIT_STATUS_FORK);
		}
	}


	/* Because of limitations in kqueue on freebsd, we must fork BEFORE we
	 * initialize the socket engine.
	 */
	SocketEngineFactory* SEF = new SocketEngineFactory();
	SE = SEF->Create(this);
	delete SEF;

	this->Modes = new ModeParser(this);
	this->AddServerName(Config->ServerName);
	CheckDie();
	int bounditems = BindPorts(true, found_ports, pl);

	for(int t = 0; t < 255; t++)
		Config->global_implementation[t] = 0;

	memset(&Config->implement_lists,0,sizeof(Config->implement_lists));

	printf("\n");

	this->Res = new DNS(this);

	this->LoadAllModules();
	/* Just in case no modules were loaded - fix for bug #101 */
	this->BuildISupport();
	InitializeDisabledCommands(Config->DisabledCommands, this);

	if ((Config->ports.size() == 0) && (found_ports > 0))
	{
		printf("\nERROR: I couldn't bind any ports! Are you sure you didn't start InspIRCd twice?\n");
		Log(DEFAULT,"ERROR: I couldn't bind any ports! Are you sure you didn't start InspIRCd twice?");
		Exit(EXIT_STATUS_BIND);
	}

	if (Config->ports.size() != (unsigned int)found_ports)
	{
		printf("\nWARNING: Not all your client ports could be bound --\nstarting anyway with %d of %d client ports bound.\n\n", bounditems, found_ports);
		printf("The following port(s) failed to bind:\n");
		int j = 1;
		for (FailedPortList::iterator i = pl.begin(); i != pl.end(); i++, j++)
		{
			printf("%d.\tIP: %s\tPort: %lu\n", j, i->first.empty() ? "<all>" : i->first.c_str(), (unsigned long)i->second);
		}
	}
#ifndef WINDOWS
	if (!Config->nofork)
	{
		if (kill(getppid(), SIGTERM) == -1)
		{
			printf("Error killing parent process: %s\n",strerror(errno));
			Log(DEFAULT,"Error killing parent process: %s",strerror(errno));
		}
	}

	if (isatty(0) && isatty(1) && isatty(2))
	{
		/* We didn't start from a TTY, we must have started from a background process -
		 * e.g. we are restarting, or being launched by cron. Dont kill parent, and dont
		 * close stdin/stdout
		 */
		if (!do_nofork)
		{
			fclose(stdin);
			fclose(stderr);
			fclose(stdout);
		}
		else
		{
			Log(DEFAULT,"Keeping pseudo-tty open as we are running in the foreground.");
		}
	}
#else
	InitIPC();
	if(!Config->nofork)
	{
		WindowsForkKillOwner(this);
		FreeConsole();
	}
#endif
	printf("\nInspIRCd is now running!\n");
	Log(DEFAULT,"Startup complete.");

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
		snprintf(versiondata,MAXBUF,"%s %s :%s [FLAGS=%s,%s,%s]",VERSION,Config->ServerName,SYSTEM,REVISION,SE->GetName().c_str(),dnsengine);
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
	v << "WALLCHOPS WALLVOICES MODES=" << MAXMODES-1 << " CHANTYPES=# PREFIX=" << this->Modes->BuildPrefixes() << " MAP MAXCHANNELS=" << Config->MaxChans << " MAXBANS=60 VBANLIST NICKLEN=" << NICKMAX-1;
	v << " CASEMAPPING=rfc1459 STATUSMSG=@%+ CHARSET=ascii TOPICLEN=" << MAXTOPIC << " KICKLEN=" << MAXKICK << " MAXTARGETS=" << Config->MaxTargets << " AWAYLEN=";
	v << MAXAWAY << " CHANMODES=" << this->Modes->ChanModes() << " FNC NETWORK=" << Config->Network << " MAXPARA=32 ELIST=MU";
	Config->data005 = v.str();
	FOREACH_MOD_I(this,I_On005Numeric,On005Numeric(Config->data005));
	Config->Update005();
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
			std::pair<int,std::string> intercount = GetInterfaceInstanceCount(modules[j]);
			if (intercount.first > 0)
			{
				this->Log(DEFAULT,"Failed to unload module %s, being used by %d other(s) via interface '%s'",filename, intercount.first, intercount.second.c_str());
				snprintf(MODERR,MAXBUF,"Module not unloadable (Still in use by %d other module%s which %s using its interface '%s') -- unload dependent modules first!",
						intercount.first,
						intercount.first > 1 ? "s" : "",
						intercount.first > 1 ? "are" : "is",
						intercount.second.c_str());
				return false;
			}
			/* Give the module a chance to tidy out all its metadata */
			for (chan_hash::iterator c = this->chanlist->begin(); c != this->chanlist->end(); c++)
			{
				modules[j]->OnCleanup(TYPE_CHANNEL,c->second);
			}
			for (user_hash::iterator u = this->clientlist->begin(); u != this->clientlist->end(); u++)
			{
				modules[j]->OnCleanup(TYPE_USER,u->second);
			}

			/* Tidy up any dangling resolvers */
			this->Res->CleanResolvers(modules[j]);

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
			Parser->RemoveCommands(filename);
			this->EraseModule(j);
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
	/* Do we have a glob pattern in the filename?
	 * The user wants to load multiple modules which
	 * match the pattern.
	 */
	if (strchr(filename,'*') || (strchr(filename,'?')))
	{
		int n_match = 0;
		DIR* library = opendir(Config->ModPath);
		if (library)
		{
			/* Try and locate and load all modules matching the pattern */
			dirent* entry = NULL;
			while ((entry = readdir(library)))
			{
				if (this->MatchText(entry->d_name, filename))
				{
					if (!this->LoadModule(entry->d_name))
						n_match++;
				}
			}
			closedir(library);
		}
		/* Loadmodule will now return false if any one of the modules failed
		 * to load (but wont abort when it encounters a bad one) and when 1 or
		 * more modules were actually loaded.
		 */
		return (n_match > 0);
	}

	char modfile[MAXBUF];
	snprintf(modfile,MAXBUF,"%s/%s",Config->ModPath,filename);
	std::string filename_str = filename;

	if (!ServerConfig::DirValid(modfile))
	{
		this->Log(DEFAULT,"Module %s is not within the modules directory.",modfile);
		snprintf(MODERR,MAXBUF,"Module %s is not within the modules directory.",modfile);
		return false;
	}
	if (ServerConfig::FileExists(modfile))
	{

		for (unsigned int j = 0; j < Config->module_names.size(); j++)
		{
			if (Config->module_names[j] == filename_str)
			{
				this->Log(DEFAULT,"Module %s is already loaded, cannot load a module twice!",modfile);
				snprintf(MODERR,MAXBUF,"Module already loaded");
				return false;
			}
		}
		Module* m = NULL;
		ircd_module* a = NULL;
		try
		{
			a = new ircd_module(this, modfile);
			factory[this->ModCount+1] = a;
			if (factory[this->ModCount+1]->LastError())
			{
				this->Log(DEFAULT,"Unable to load %s: %s",modfile,factory[this->ModCount+1]->LastError());
				snprintf(MODERR,MAXBUF,"Loader/Linker error: %s",factory[this->ModCount+1]->LastError());
				return false;
			}
			if ((long)factory[this->ModCount+1]->factory != -1)
			{
				m = factory[this->ModCount+1]->factory->CreateModule(this);

				Version v = m->GetVersion();

				if (v.API != API_VERSION)
				{
					delete m;
					this->Log(DEFAULT,"Unable to load %s: Incorrect module API version: %d (our version: %d)",modfile,v.API,API_VERSION);
					snprintf(MODERR,MAXBUF,"Loader/Linker error: Incorrect module API version: %d (our version: %d)",v.API,API_VERSION);
					return false;
				}
				else
				{
					this->Log(DEFAULT,"New module introduced: %s (API version %d, Module version %d.%d.%d.%d)%s", filename, v.API, v.Major, v.Minor, v.Revision, v.Build, (!(v.Flags & VF_VENDOR) ? " [3rd Party]" : " [Vendor]"));
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
		catch (CoreException& modexcept)
		{
			this->Log(DEFAULT,"Unable to load %s: %s",modfile,modexcept.GetReason());
			snprintf(MODERR,MAXBUF,"Factory function of %s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
			return false;
		}
	}
	else
	{
		this->Log(DEFAULT,"InspIRCd: startup: Module Not Found %s",modfile);
		snprintf(MODERR,MAXBUF,"Module file could not be found");
		return false;
	}
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
#ifndef WIN32
	static rusage ru;
#else
	static time_t uptime;
	static struct tm * stime;
	static char window_title[100];
#endif

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
			this->RehashUsersAndChans();
			FOREACH_MOD_I(this, I_OnGarbageCollect, OnGarbageCollect());
		}
		Timers->TickTimers(TIME);
		this->DoBackgroundUserStuff(TIME);

		if ((TIME % 5) == 0)
		{
			XLines->expire_lines();
			FOREACH_MOD_I(this,I_OnBackgroundTimer,OnBackgroundTimer(TIME));
			Timers->TickMissedTimers(TIME);
		}
#ifndef WIN32
		/* Same change as in cmd_stats.cpp, use RUSAGE_SELF rather than '0' -- Om */
		if (!getrusage(RUSAGE_SELF, &ru))
		{
			gettimeofday(&this->stats->LastSampled, NULL);
			this->stats->LastCPU = ru.ru_utime;
		}
#else
		CheckIPC(this);

		if(Config->nofork)
		{
			uptime = Time() - startup_time;
			stime = gmtime(&uptime);
			snprintf(window_title, 100, "InspIRCd - %u clients, %u accepted connections - Up %u days, %.2u:%.2u:%.2u",
				LocalUserCount(), stats->statsAccept, stime->tm_yday, stime->tm_hour, stime->tm_min, stime->tm_sec);
			SetConsoleTitle(window_title);
		}
#endif
	}

	/* Call the socket engine to wait on the active
	 * file descriptors. The socket engine has everything's
	 * descriptors in its list... dns, modules, users,
	 * servers... so its nice and easy, just one call.
	 * This will cause any read or write events to be
	 * dispatched to their handlers.
	 */
	SE->DispatchEvents();

	/* if any users was quit, take them out */
	GlobalCulls.Apply();

	/* If any inspsockets closed, remove them */
	this->InspSocketCull();
}

void InspIRCd::InspSocketCull()
{
	for (std::map<InspSocket*,InspSocket*>::iterator x = SocketCull.begin(); x != SocketCull.end(); ++x)
	{
		SE->DelFd(x->second);
		x->second->Close();
		delete x->second;
	}
	SocketCull.clear();
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

time_t InspIRCd::Time(bool delta)
{
	if (delta)
		return TIME + time_delta;
	return TIME;
}

int InspIRCd::SetTimeDelta(int delta)
{
	int old = time_delta;
	time_delta = delta;
	this->Log(DEBUG, "Time delta set to %d (was %d)", time_delta, old);
	return old;
}

void InspIRCd::AddLocalClone(userrec* user)
{
	clonemap::iterator x = local_clones.find(user->GetIPString());
	if (x != local_clones.end())
		x->second++;
	else
		local_clones[user->GetIPString()] = 1;
}

void InspIRCd::AddGlobalClone(userrec* user)
{
	clonemap::iterator y = global_clones.find(user->GetIPString());
	if (y != global_clones.end())
		y->second++;
	else
		global_clones[user->GetIPString()] = 1;
}

int InspIRCd::GetTimeDelta()
{
	return time_delta;
}

bool FileLogger::Readable()
{
	return false;
}

void FileLogger::HandleEvent(EventType et, int errornum)
{
	this->WriteLogLine("");
	if (log)
		ServerInstance->SE->DelFd(this);
}

void FileLogger::WriteLogLine(const std::string &line)
{
	if (line.length())
		buffer.append(line);

	if (log)
	{
		int written = fprintf(log,"%s",buffer.c_str());
#ifdef WINDOWS
		buffer.clear();
#else
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
			buffer.clear();
		}
#endif
		if (writeops++ % 20)
		{
			fflush(log);
		}
	}
}

void FileLogger::Close()
{
	if (log)
	{
		/* Burlex: Windows assumes nonblocking on FILE* pointers anyway, and also "file" fd's aren't the same
		 * as socket fd's. */
#ifndef WIN32
		int flags = fcntl(fileno(log), F_GETFL, 0);
		fcntl(fileno(log), F_SETFL, flags ^ O_NONBLOCK);
#endif
		if (buffer.size())
			fprintf(log,"%s",buffer.c_str());

#ifndef WINDOWS
		ServerInstance->SE->DelFd(this);
#endif

		fflush(log);
		fclose(log);
	}

	buffer.clear();
}

FileLogger::FileLogger(InspIRCd* Instance, FILE* logfile) : ServerInstance(Instance), log(logfile), writeops(0)
{
	if (log)
	{
		irc::sockets::NonBlocking(fileno(log));
		this->SetFd(fileno(log));
		buffer.clear();
	}
}

FileLogger::~FileLogger()
{
	this->Close();
}

