/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core */
/* $Install: src/inspircd $(BINPATH) */
#include "inspircd.h"
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

	#include <pwd.h> // setuid
	#include <grp.h> // setgid
#endif

#include <fstream>
#include "xline.h"
#include "bancache.h"
#include "socketengine.h"
#include "inspircd_se_config.h"
#include "socket.h"
#include "command_parse.h"
#include "exitcodes.h"
#include "caller.h"
#include "testsuite.h"

InspIRCd* SI = NULL;
int* mysig = NULL;

/** Seperate from the other casemap tables so that code *can* still exclusively rely on RFC casemapping
 * if it must.
 *
 * This is provided as a pointer so that modules can change it to their custom mapping tables,
 * e.g. for national character support.
 */
unsigned const char *national_case_insensitive_map = rfc_case_insensitive_map;


/* Moved from exitcodes.h -- due to duplicate symbols -- Burlex
 * XXX this is a bit ugly. -- w00t
 */
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
		"Bad command handler loaded", /* 16 */
		"RegisterServiceCtrlHandler failed", /* 17 */
		"UpdateSCMStatus failed", /* 18 */
		"CreateEvent failed" /* 19 */
};

template<typename T> static void DeleteZero(T* n)
{
	if (n != NULL)
	{
		delete n;
		n = NULL;
	}
}

void InspIRCd::Cleanup()
{
	if (Config)
	{
		for (unsigned int i = 0; i < ports.size(); i++)
		{
			/* This calls the constructor and closes the listening socket */
			delete ports[i];
		}

		ports.clear();
	}

	/* Close all client sockets, or the new process inherits them */
	for (std::vector<User*>::const_iterator i = this->Users->local_users.begin(); i != this->Users->local_users.end(); i++)
	{
		this->Users->QuitUser((*i), "Server shutdown");
		(*i)->CloseSocket();
	}

	/* We do this more than once, so that any service providers get a
	 * chance to be unhooked by the modules using them, but then get
	 * a chance to be removed themsleves.
	 *
	 * XXX there may be a better way to do this with 1.2
	 */
	for (int tries = 0; tries < 3; tries++)
	{
		std::vector<std::string> module_names = Modules->GetAllModuleNames(0);
		for (std::vector<std::string>::iterator k = module_names.begin(); k != module_names.end(); ++k)
		{
			/* Unload all modules, so they get a chance to clean up their listeners */
			this->Modules->Unload(k->c_str());
		}
	}
	/* Remove core commands */
	Parser->RemoveCommands("<core>");

	/* Cleanup Server Names */
	for(servernamelist::iterator itr = servernames.begin(); itr != servernames.end(); ++itr)
		delete (*itr);

	/* Delete objects dynamically allocated in constructor (destructor would be more appropriate, but we're likely exiting) */
	/* Must be deleted before modes as it decrements modelines */
	DeleteZero(this->Users);
	DeleteZero(this->Modes);
	DeleteZero(this->XLines);
	DeleteZero(this->Parser);
	DeleteZero(this->stats);
	DeleteZero(this->Modules);
	DeleteZero(this->BanCache);
	DeleteZero(this->SNO);
	DeleteZero(this->Config);
	DeleteZero(this->Res);
	DeleteZero(this->chanlist);
	DeleteZero(this->PI);
	DeleteZero(this->Threads);
	DeleteZero(this->Timers);
	/* Close logging */
	this->Logs->CloseLogs();
	DeleteZero(this->Logs);
}

void InspIRCd::Restart(const std::string &reason)
{
	/* SendError flushes each client's queue,
	 * regardless of writeability state
	 */
	this->SendError(reason);

	/* Figure out our filename (if theyve renamed it, we're boned) */
	std::string me;

#ifdef WINDOWS
	char module[MAX_PATH];
	if (GetModuleFileName(NULL, module, MAX_PATH))
		me = module;
#else
	me = Config->MyDir + "/inspircd";
#endif

	char** argv = Config->argv;

	this->Cleanup();

	if (execv(me.c_str(), argv) == -1)
	{
		/* Will raise a SIGABRT if not trapped */
		throw CoreException(std::string("Failed to execv()! error: ") + strerror(errno));
	}
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
	user_hash* old_users = this->Users->clientlist;
	user_hash* old_uuid  = this->Users->uuidlist;
	chan_hash* old_chans = this->chanlist;

	this->Users->clientlist = new user_hash();
	this->Users->uuidlist = new user_hash();
	this->chanlist = new chan_hash();

	for (user_hash::const_iterator n = old_users->begin(); n != old_users->end(); n++)
		this->Users->clientlist->insert(*n);

	delete old_users;

	for (user_hash::const_iterator n = old_uuid->begin(); n != old_uuid->end(); n++)
		this->Users->uuidlist->insert(*n);

	delete old_uuid;

	for (chan_hash::const_iterator n = old_chans->begin(); n != old_chans->end(); n++)
		this->chanlist->insert(*n);

	delete old_chans;
}

void InspIRCd::SetSignals()
{
#ifndef WIN32
	signal(SIGALRM, SIG_IGN);
	signal(SIGHUP, InspIRCd::SetSignal);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
	/* We want E2BIG not a signal! */
	signal(SIGXFSZ, SIG_IGN);
#endif
	signal(SIGTERM, InspIRCd::SetSignal);
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

	signal(SIGTERM, InspIRCd::SetSignal);

	rlimit rl;
	if (getrlimit(RLIMIT_CORE, &rl) == -1)
	{
		this->Logs->Log("STARTUP",DEFAULT,"Failed to getrlimit()!");
		return false;
	}
	else
	{
		rl.rlim_cur = rl.rlim_max;
		if (setrlimit(RLIMIT_CORE, &rl) == -1)
			this->Logs->Log("STARTUP",DEFAULT,"setrlimit() failed, cannot increase coredump size.");
	}

	return true;
#endif
}

void InspIRCd::WritePID(const std::string &filename)
{
	std::string fname = (filename.empty() ? "inspircd.pid" : filename);
	std::replace(fname.begin(), fname.end(), '\\', '/');
	if ((fname[0] != '/') && (!Config->StartsWithWindowsDriveLetter(filename)))
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
		this->Logs->Log("STARTUP",DEFAULT,"Failed to write PID-file '%s', exiting.",fname.c_str());
		Exit(EXIT_STATUS_PID);
	}
}

InspIRCd::InspIRCd(int argc, char** argv)
	: GlobalCulls(this),

	 /* Functor initialisation. Note that the ordering here is very important.
	  *
	  * THIS MUST MATCH ORDER OF DECLARATION OF THE HandleWhateverFunc classes
	  * within class InspIRCd.
	  */
	 HandleProcessUser(this),
	 HandleIsNick(this),
	 HandleIsIdent(this),
	 HandleFloodQuitUser(this),
	 HandleIsChannel(this),
	 HandleIsSID(this),
	 HandleRehash(this),

	 /* Functor pointer initialisation. Must match the order of the list above
	  *
	  * THIS MUST MATCH THE ORDER OF DECLARATION OF THE FUNCTORS, e.g. the methods
	  * themselves within the class.
	  */
	 ProcessUser(&HandleProcessUser),
	 IsChannel(&HandleIsChannel),
	 IsSID(&HandleIsSID),
	 Rehash(&HandleRehash),
	 IsNick(&HandleIsNick),
	 IsIdent(&HandleIsIdent),
	 FloodQuitUser(&HandleFloodQuitUser)

{
#ifdef WIN32
	// Strict, frequent checking of memory on debug builds
	_CrtSetDbgFlag ( _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );

	// Avoid erroneous frees on early exit
	WindowsIPC = 0;
#endif
	FailedPortList pl;
	int do_version = 0, do_nofork = 0, do_debug = 0,
	    do_nolog = 0, do_root = 0, do_testsuite = 0;    /* flag variables */
	int c = 0;

	// Initialize so that if we exit before proper initialization they're not deleted
	this->Logs = 0;
	this->Threads = 0;
	this->PI = 0;
	this->Users = 0;
	this->chanlist = 0;
	this->Config = 0;
	this->SNO = 0;
	this->BanCache = 0;
	this->Modules = 0;
	this->stats = 0;
	this->Timers = 0;
	this->Parser = 0;
	this->XLines = 0;
	this->Modes = 0;
	this->Res = 0;
	this->ConfigThread = NULL;

	// Initialise TIME
	this->TIME = time(NULL);

	memset(&server, 0, sizeof(server));
	memset(&client, 0, sizeof(client));

	// This must be created first, so other parts of Insp can use it while starting up
	this->Logs = new LogManager(this);

	SocketEngineFactory* SEF = new SocketEngineFactory();
	SE = SEF->Create(this);
	delete SEF;

	this->Threads = new ThreadEngine(this);

	/* Default implementation does nothing */
	this->PI = new ProtocolInterface(this);

	this->s_signal = 0;

	// Create base manager classes early, so nothing breaks
	this->Users = new UserManager(this);

	this->Users->unregistered_count = 0;

	this->Users->clientlist = new user_hash();
	this->Users->uuidlist = new user_hash();
	this->chanlist = new chan_hash();

	this->Config = new ServerConfig(this);
	this->SNO = new SnomaskManager(this);
	this->BanCache = new BanCacheManager(this);
	this->Modules = new ModuleManager(this);
	this->stats = new serverstats();
	this->Timers = new TimerManager(this);
	this->Parser = new CommandParser(this);
	this->XLines = new XLineManager(this);

	this->Config->argv = argv;
	this->Config->argc = argc;

	if (chdir(Config->GetFullProgDir().c_str()))
	{
		printf("Unable to change to my directory: %s\nAborted.", strerror(errno));
		exit(0);
	}

	this->Config->opertypes.clear();
	this->Config->operclass.clear();

	this->TIME = this->OLDTIME = this->startup_time = time(NULL);
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
		{ "testsuite",	no_argument,		&do_testsuite,	1	},
		{ 0, 0, 0, 0 }
	};

	int index;
	while ((c = getopt_long_only(argc, argv, ":f:", longopts, &index)) != -1)
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
			case '?':
				/* Unknown parameter */
			default:
				/* Fall through to handle other weird values too */
				printf("Unknown parameter '%s'\n", argv[optind-1]);
				printf("Usage: %s [--nofork] [--nolog] [--debug] [--logfile <filename>]\n%*s[--runasroot] [--version] [--config <config>] [--testsuite]\n", argv[0], static_cast<int>(8+strlen(argv[0])), " ");
				Exit(EXIT_STATUS_ARGV);
			break;
		}
	}

	if (do_testsuite)
		do_nofork = do_debug = true;

	if (do_version)
	{
		printf("\n%s r%s\n", VERSION, REVISION);
		Exit(EXIT_STATUS_NOERROR);
	}

#ifdef WIN32

	// Handle forking
	if(!do_nofork)
	{
		DWORD ExitCode = WindowsForkStart(this);
		if(ExitCode)
			exit(ExitCode);
	}

	// Set up winsock
	WSADATA wsadata;
	WSAStartup(MAKEWORD(2,0), &wsadata);
	ChangeWindowsSpecificPointers(this);
#endif
	Config->MyExecutable = argv[0];

	/* Set the finished argument values */
	Config->nofork = do_nofork;
	Config->forcedebug = do_debug;
	Config->writelog = !do_nolog;
	Config->TestSuite = do_testsuite;

	if (!this->OpenLog(argv, argc))
	{
		printf("ERROR: Could not open logfile %s: %s\n\n", Config->logpath.c_str(), strerror(errno));
		Exit(EXIT_STATUS_LOG);
	}

	if (!ServerConfig::FileExists(this->ConfigFileName))
	{
#ifdef WIN32
		/* Windows can (and defaults to) hide file extensions, so let's play a bit nice for windows users. */
		std::string txtconf = this->ConfigFileName;
		txtconf.append(".txt");

		if (ServerConfig::FileExists(txtconf.c_str()))
		{
			strlcat(this->ConfigFileName, ".txt", MAXBUF);
		}
		else
#endif
		{
			printf("ERROR: Cannot open config file: %s\nExiting...\n", this->ConfigFileName);
			this->Logs->Log("STARTUP",DEFAULT,"Unable to open config file %s", this->ConfigFileName);
			Exit(EXIT_STATUS_CONFIG);
		}
	}

	printf_c("\033[1;32mInspire Internet Relay Chat Server, compiled %s at %s\n",__DATE__,__TIME__);
	printf_c("(C) InspIRCd Development Team.\033[0m\n\n");
	printf_c("Developers:\n");
	printf_c("\t\033[1;32mBrain, FrostyCoolSlug, w00t, Om, Special, peavey\n");
	printf_c("\t\033[1;32maquanight, psychon, dz, danieldg, jackmcbarn\033[0m\n\n");
	printf_c("Others:\t\t\t\033[1;32mSee /INFO Output\033[0m\n");

	this->Modes = new ModeParser(this);

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
			Logs->Log("STARTUP", DEFAULT, "ERROR: could not go into daemon mode. Shutting down.");
			Exit(EXIT_STATUS_FORK);
		}
	}

	SE->RecoverFromFork();

	/* During startup we don't actually initialize this
	 * in the thread engine.
	 */
	this->Config->Read();
	this->Config->Apply(NULL, "");
	Logs->OpenFileLogs();

	/** Note: This is safe, the method checks for user == NULL */
	this->Parser->SetupCommandTable();

	this->Res = new DNS(this);

	this->AddServerName(Config->ServerName);

	/*
	 * Initialise SID/UID.
 	 * For an explanation as to exactly how this works, and why it works this way, see GetUID().
	 *   -- w00t
 	 */
	if (!*Config->sid)
	{
		// Generate one
		size_t sid = 0;

		for (const char* x = Config->ServerName; *x; ++x)
			sid = 5 * sid + *x;
		for (const char* y = Config->ServerDesc; *y; ++y)
			sid = 5 * sid + *y;
		sid = sid % 999;

		Config->sid[0] = (char)(sid / 100 + 48);
		Config->sid[1] = (char)(((sid / 10) % 10) + 48);
		Config->sid[2] = (char)(sid % 10 + 48);
		Config->sid[3] = '\0';
	}

	/* set up fake client again this time with the correct uid */
	this->FakeClient = new FakeUser(this, "!");
	this->FakeClient->SetFakeServer(Config->ServerName);

	// Get XLine to do it's thing.
	this->XLines->CheckELines();
	this->XLines->ApplyLines();

	int bounditems = BindPorts(pl);

	printf("\n");

	this->Modules->LoadAll();

	/* Just in case no modules were loaded - fix for bug #101 */
	this->BuildISupport();
	Config->ApplyDisabledCommands(Config->DisabledCommands);

	if (!pl.empty())
	{
		printf("\nWARNING: Not all your client ports could be bound --\nstarting anyway with %d of %d client ports bound.\n\n",
			bounditems, bounditems + (int)pl.size());
		printf("The following port(s) failed to bind:\n");
		printf("Hint: Try using a public IP instead of blank or *\n\n");
		int j = 1;
		for (FailedPortList::iterator i = pl.begin(); i != pl.end(); i++, j++)
		{
			printf("%d.\tAddress: %s\tReason: %s\n", j, i->first.empty() ? "<all>" : i->first.c_str(), i->second.c_str());
		}
	}

	printf("\nInspIRCd is now running as '%s'[%s] with %d max open sockets\n", Config->ServerName,Config->GetSID().c_str(), SE->GetMaxFds());

#ifndef WINDOWS
	if (!Config->nofork)
	{
		if (kill(getppid(), SIGTERM) == -1)
		{
			printf("Error killing parent process: %s\n",strerror(errno));
			Logs->Log("STARTUP", DEFAULT, "Error killing parent process: %s",strerror(errno));
		}
	}

    /* Explicitly shut down stdio's stdin/stdout/stderr.
     *
     * The previous logic here was to only do this if stdio was connected to a controlling
     * terminal.  However, we must do this always to avoid information leaks and other
     * problems related to stdio.
     *
     * The only exception is if we are in debug mode.
     *
     *    -- nenolod
     */
    if ((!do_nofork) && (!do_testsuite) && (!Config->forcedebug))
    {
        int fd;

        fclose(stdin);
        fclose(stderr);
        fclose(stdout);

        fd = open("/dev/null", O_RDWR);
        if (dup2(fd, 0) < 0)
            Logs->Log("STARTUP", DEFAULT, "Failed to dup /dev/null to stdin.");
        if (dup2(fd, 1) < 0)
            Logs->Log("STARTUP", DEFAULT, "Failed to dup /dev/null to stdout.");
        if (dup2(fd, 2) < 0)
            Logs->Log("STARTUP", DEFAULT, "Failed to dup /dev/null to stderr.");
        close(fd);
    }
    else
    {
        Logs->Log("STARTUP", DEFAULT,"Keeping pseudo-tty open as we are running in the foreground.");
    }
#else
	WindowsIPC = new IPC(this);
	if(!Config->nofork)
	{
		WindowsForkKillOwner(this);
		FreeConsole();
	}
	/* Set win32 service as running, if we are running as a service */
	SetServiceRunning();
#endif

	Logs->Log("STARTUP", DEFAULT, "Startup complete as '%s'[%s], %d max open sockets", Config->ServerName,Config->GetSID().c_str(), SE->GetMaxFds());

#ifndef WIN32
	if (*(this->Config->SetGroup))
	{
		int ret;

		// setgroups
		ret = setgroups(0, NULL);

		if (ret == -1)
		{
			this->Logs->Log("SETGROUPS", DEFAULT, "setgroups() failed (wtf?): %s", strerror(errno));
			this->QuickExit(0);
		}

		// setgid
		struct group *g;

		errno = 0;
		g = getgrnam(this->Config->SetGroup);

		if (!g)
		{
			this->Logs->Log("SETGUID", DEFAULT, "getgrnam() failed (bad user?): %s", strerror(errno));
			this->QuickExit(0);
		}

		ret = setgid(g->gr_gid);

		if (ret == -1)
		{
			this->Logs->Log("SETGUID", DEFAULT, "setgid() failed (bad user?): %s", strerror(errno));
			this->QuickExit(0);
		}
	}

	if (*(this->Config->SetUser))
	{
		// setuid
		struct passwd *u;

		errno = 0;
		u = getpwnam(this->Config->SetUser);

		if (!u)
		{
			this->Logs->Log("SETGUID", DEFAULT, "getpwnam() failed (bad user?): %s", strerror(errno));
			this->QuickExit(0);
		}

		int ret = setuid(u->pw_uid);

		if (ret == -1)
		{
			this->Logs->Log("SETGUID", DEFAULT, "setuid() failed (bad user?): %s", strerror(errno));
			this->QuickExit(0);
		}
	}
#endif

	this->WritePID(Config->PID);
}

int InspIRCd::Run()
{
	/* See if we're supposed to be running the test suite rather than entering the mainloop */
	if (Config->TestSuite)
	{
		TestSuite* ts = new TestSuite(this);
		delete ts;
		Exit(0);
	}

	while (true)
	{
#ifndef WIN32
		static rusage ru;
#else
		static time_t uptime;
		static struct tm * stime;
		static char window_title[100];
#endif

		/* Check if there is a config thread which has finished executing but has not yet been freed */
		if (this->ConfigThread && this->ConfigThread->IsDone())
		{
			/* Rehash has completed */
			this->Logs->Log("CONFIG",DEBUG,"Detected ConfigThread exiting, tidying up...");

			this->ConfigThread->Finish();

			ConfigThread->join();
			delete ConfigThread;
			ConfigThread = NULL;
		}

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
			/* Allow a buffer of two seconds drift on this so that ntpdate etc dont harass admins */
			if (TIME < OLDTIME - 2)
			{
				SNO->WriteToSnoMask('d', "\002EH?!\002 -- Time is flowing BACKWARDS in this dimension! Clock drifted backwards %lu secs.", (unsigned long)OLDTIME-TIME);
			}
			else if (TIME > OLDTIME + 2)
			{
				SNO->WriteToSnoMask('d', "\002EH?!\002 -- Time is jumping FORWARDS! Clock skipped %lu secs.", (unsigned long)TIME - OLDTIME);
			}

			if ((TIME % 3600) == 0)
			{
				this->RehashUsersAndChans();
				FOREACH_MOD_I(this, I_OnGarbageCollect, OnGarbageCollect());
			}

			Timers->TickTimers(TIME);
			this->DoBackgroundUserStuff();

			if ((TIME % 5) == 0)
			{
				FOREACH_MOD_I(this,I_OnBackgroundTimer,OnBackgroundTimer(TIME));
				SNO->FlushSnotices();
			}
#ifndef WIN32
			/* Same change as in cmd_stats.cpp, use RUSAGE_SELF rather than '0' -- Om */
			if (!getrusage(RUSAGE_SELF, &ru))
			{
				gettimeofday(&this->stats->LastSampled, NULL);
				this->stats->LastCPU = ru.ru_utime;
			}
#else
			WindowsIPC->Check();
#endif
		}

		/* Call the socket engine to wait on the active
		 * file descriptors. The socket engine has everything's
		 * descriptors in its list... dns, modules, users,
		 * servers... so its nice and easy, just one call.
		 * This will cause any read or write events to be
		 * dispatched to their handlers.
		 */
		this->SE->DispatchEvents();

		/* if any users were quit, take them out */
		this->GlobalCulls.Apply();

		/* If any inspsockets closed, remove them */
		this->BufferedSocketCull();

		if (this->s_signal)
		{
			this->SignalHandler(s_signal);
			this->s_signal = 0;
		}
	}

	return 0;
}

void InspIRCd::BufferedSocketCull()
{
	for (std::map<BufferedSocket*,BufferedSocket*>::iterator x = SocketCull.begin(); x != SocketCull.end(); ++x)
	{
		this->Logs->Log("CULLLIST",DEBUG,"Cull socket");
		SE->DelFd(x->second);
		x->second->Close();
		delete x->second;
	}
	SocketCull.clear();
}

/**********************************************************************************/

/**
 * An ircd in five lines! bwahahaha. ahahahahaha. ahahah *cough*.
 */

/* this returns true when all modules are satisfied that the user should be allowed onto the irc server
 * (until this returns true, a user will block in the waiting state, waiting to connect up to the
 * registration timeout maximum seconds)
 */
bool InspIRCd::AllModulesReportReady(User* user)
{
	for (EventHandlerIter i = Modules->EventHandlers[I_OnCheckReady].begin(); i != Modules->EventHandlers[I_OnCheckReady].end(); ++i)
	{
		if (!(*i)->OnCheckReady(user))
			return false;
	}
	return true;
}

time_t InspIRCd::Time()
{
	return TIME;
}

void InspIRCd::SetSignal(int signal)
{
	*mysig = signal;
}

/* On posix systems, the flow of the program starts right here, with
 * ENTRYPOINT being a #define that defines main(). On Windows, ENTRYPOINT
 * defines smain() and the real main() is in the service code under
 * win32service.cpp. This allows the service control manager to control
 * the process where we are running as a windows service.
 */
ENTRYPOINT
{
	SI = new InspIRCd(argc, argv);
	mysig = &SI->s_signal;
	SI->Run();
	delete SI;
	return 0;
}
