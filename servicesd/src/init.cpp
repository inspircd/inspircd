/* Initialization and related routines.
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

#include "services.h"
#include "config.h"
#include "users.h"
#include "protocol.h"
#include "bots.h"
#include "xline.h"
#include "socketengine.h"
#include "servers.h"
#include "language.h"

#ifndef _WIN32
#include <sys/wait.h>
#include <sys/stat.h>

#include <errno.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#endif

Anope::string Anope::ConfigDir = "conf", Anope::DataDir = "data",
                     Anope::ModuleDir = "lib", Anope::LocaleDir = "locale", Anope::LogDir = "logs";

/* Vector of pairs of command line arguments and their params */
static std::vector<std::pair<Anope::string, Anope::string> >
CommandLineArguments;

/** Called on startup to organize our starting arguments in a better way
 * and check for errors
 * @param ac number of args
 * @param av args
 */
static void ParseCommandLineArguments(int ac, char **av) {
    for (int i = 1; i < ac; ++i) {
        Anope::string option = av[i];
        Anope::string param;
        while (!option.empty() && option[0] == '-') {
            option.erase(option.begin());
        }
        size_t t = option.find('=');
        if (t != Anope::string::npos) {
            param = option.substr(t + 1);
            option.erase(t);
        }

        if (option.empty()) {
            continue;
        }

        CommandLineArguments.push_back(std::make_pair(option, param));
    }
}

/** Check if an argument was given on startup and its parameter
 * @param name The argument name
 * @param shortname A shorter name, eg --debug and -d
 * @param param A string to put the param, if any, of the argument
 * @return true if name/shortname was found, false if not
 */
static bool GetCommandLineArgument(const Anope::string &name, char shortname,
                                   Anope::string &param) {
    param.clear();

    for (std::vector<std::pair<Anope::string, Anope::string> >::iterator it =
                CommandLineArguments.begin(), it_end = CommandLineArguments.end(); it != it_end;
            ++it) {
        if (it->first.equals_ci(name) || (it->first.length() == 1
                                          && it->first[0] == shortname)) {
            param = it->second;
            return true;
        }
    }

    return false;
}

/** Check if an argument was given on startup
 * @param name The argument name
 * @param shortname A shorter name, eg --debug and -d
 * @return true if name/shortname was found, false if not
 */
static bool GetCommandLineArgument(const Anope::string &name,
                                   char shortname = 0) {
    Anope::string Unused;
    return GetCommandLineArgument(name, shortname, Unused);
}

bool Anope::AtTerm() {
    return isatty(fileno(stdout)) && isatty(fileno(stdin))
           && isatty(fileno(stderr));
}

static void setuidgid();

void Anope::Fork() {
#ifndef _WIN32
    kill(getppid(), SIGUSR2);

    if (!freopen("/dev/null", "r", stdin)) {
        Log() << "Unable to redirect stdin to /dev/null: " << Anope::LastError();
    }
    if (!freopen("/dev/null", "w", stdout)) {
        Log() << "Unable to redirect stdout to /dev/null: " << Anope::LastError();
    }
    if (!freopen("/dev/null", "w", stderr)) {
        Log() << "Unable to redirect stderr to /dev/null: " << Anope::LastError();
    }

    setpgid(0, 0);

    setuidgid();
#else
    FreeConsole();
#endif
}

void Anope::HandleSignal() {
    switch (Signal) {
    case SIGHUP: {
        Anope::SaveDatabases();

        try {
            Configuration::Conf *new_config = new Configuration::Conf();
            Configuration::Conf *old = Config;
            Config = new_config;
            Config->Post(old);
            delete old;
        } catch (const ConfigException &ex) {
            Log() << "Error reloading configuration file: " << ex.GetReason();
        }
        break;
    }
    case SIGTERM:
    case SIGINT:
#ifndef _WIN32
        Log() << "Received " << strsignal(Signal) << " signal (" << Signal <<
              "), exiting.";
        Anope::QuitReason = Anope::string("Services terminating via signal ") +
                            strsignal(Signal) + " (" + stringify(Signal) + ")";
#else
        Log() << "Received signal " << Signal << ", exiting.";
        Anope::QuitReason = Anope::string("Services terminating via signal ") +
                            stringify(Signal);
#endif
        Anope::Quitting = true;
        Anope::SaveDatabases();
        break;
    }

    Signal = 0;
}

#ifndef _WIN32
static void parent_signal_handler(int signal) {
    if (signal == SIGUSR2) {
        Anope::Quitting = true;
    } else if (signal == SIGCHLD) {
        Anope::ReturnValue = -1;
        Anope::Quitting = true;
        int status = 0;
        wait(&status);
        if (WIFEXITED(status)) {
            Anope::ReturnValue = WEXITSTATUS(status);
        }
    }
}
#endif

static void SignalHandler(int sig) {
    Anope::Signal = sig;
}

static void InitSignals() {
    struct sigaction sa;

    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    sa.sa_handler = SignalHandler;

    sigaction(SIGHUP, &sa, NULL);

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = SIG_IGN;

#ifndef _WIN32
    sigaction(SIGCHLD, &sa, NULL);
#endif
    sigaction(SIGPIPE, &sa, NULL);
}

/* Remove our PID file.  Done at exit. */

static void remove_pidfile() {
    remove(Config->GetBlock("serverinfo")->Get<const Anope::string>("pid").c_str());
}

/* Create our PID file and write the PID to it. */

static void write_pidfile() {
    FILE *pidfile = fopen(
                        Config->GetBlock("serverinfo")->Get<const Anope::string>("pid").c_str(), "w");
    if (pidfile) {
#ifdef _WIN32
        fprintf(pidfile, "%d\n", static_cast<int>(GetCurrentProcessId()));
#else
        fprintf(pidfile, "%d\n", static_cast<int>(getpid()));
#endif
        fclose(pidfile);
        atexit(remove_pidfile);
    } else {
        throw CoreException("Can not write to PID file " +
                            Config->GetBlock("serverinfo")->Get<const Anope::string>("pid"));
    }
}

static void setuidgid() {
#ifndef _WIN32
    Configuration::Block *options = Config->GetBlock("options");
    uid_t uid = -1;
    gid_t gid = -1;

    if (!options->Get<const Anope::string>("user").empty()) {
        errno = 0;
        struct passwd *u = getpwnam(options->Get<const Anope::string>("user").c_str());
        if (u == NULL) {
            Log() << "Unable to setuid to " << options->Get<const Anope::string>("user") <<
                  ": " << Anope::LastError();
        } else {
            uid = u->pw_uid;
        }
    }
    if (!options->Get<const Anope::string>("group").empty()) {
        errno = 0;
        struct group *g = getgrnam(options->Get<const Anope::string>("group").c_str());
        if (g == NULL) {
            Log() << "Unable to setgid to " << options->Get<const Anope::string>("group") <<
                  ": " << Anope::LastError();
        } else {
            gid = g->gr_gid;
        }
    }

    for (unsigned i = 0; i < Config->LogInfos.size(); ++i) {
        LogInfo& li = Config->LogInfos[i];

        for (unsigned j = 0; j < li.logfiles.size(); ++j) {
            LogFile* lf = li.logfiles[j];

            errno = 0;
            if (chown(lf->filename.c_str(), uid, gid) != 0) {
                Log() << "Unable to change the ownership of " << lf->filename << " to " << uid
                      << "/" << gid << ": " << Anope::LastError();
            }
        }
    }

    if (static_cast<int>(gid) != -1) {
        if (setgid(gid) == -1) {
            Log() << "Unable to setgid to " << options->Get<const Anope::string>("group") <<
                  ": " << Anope::LastError();
        } else {
            Log() << "Successfully set group to " <<
                  options->Get<const Anope::string>("group");
        }
    }
    if (static_cast<int>(uid) != -1) {
        if (setuid(uid) == -1) {
            Log() << "Unable to setuid to " << options->Get<const Anope::string>("user") <<
                  ": " << Anope::LastError();
        } else {
            Log() << "Successfully set user to " <<
                  options->Get<const Anope::string>("user");
        }
    }
#endif
}

void Anope::Init(int ac, char **av) {
    /* Set file creation mask and group ID. */
#if defined(DEFUMASK) && HAVE_UMASK
    umask(DEFUMASK);
#endif

    Serialize::RegisterTypes();

    /* Parse command line arguments */
    ParseCommandLineArguments(ac, av);

    if (GetCommandLineArgument("version", 'v')) {
        Log(LOG_TERMINAL) << "Anope-" << Anope::Version() << " -- " <<
                          Anope::VersionBuildString();
        throw CoreException();
    }

    if (GetCommandLineArgument("help", 'h')) {
        Log(LOG_TERMINAL) << "Anope-" << Anope::Version() << " -- " <<
                          Anope::VersionBuildString();
        Log(LOG_TERMINAL) << "Anope IRC Services (https://www.anope.org/)";
        Log(LOG_TERMINAL) << "Usage ./" << Anope::ServicesBin << " [options] ...";
        Log(LOG_TERMINAL) << "-c, --config=filename.conf";
        Log(LOG_TERMINAL) << "    --confdir=conf file directory";
        Log(LOG_TERMINAL) << "    --dbdir=database directory";
        Log(LOG_TERMINAL) << "-d, --debug[=level]";
        Log(LOG_TERMINAL) << "-h, --help";
        Log(LOG_TERMINAL) << "    --localedir=locale directory";
        Log(LOG_TERMINAL) << "    --logdir=logs directory";
        Log(LOG_TERMINAL) << "    --modulesdir=modules directory";
        Log(LOG_TERMINAL) << "-e, --noexpire";
        Log(LOG_TERMINAL) << "-n, --nofork";
        Log(LOG_TERMINAL) << "    --nothird";
        Log(LOG_TERMINAL) << "    --protocoldebug";
        Log(LOG_TERMINAL) << "-r, --readonly";
        Log(LOG_TERMINAL) << "-s, --support";
        Log(LOG_TERMINAL) << "-v, --version";
        Log(LOG_TERMINAL) << "";
        Log(LOG_TERMINAL) << "Further support is available from https://www.anope.org/";
        Log(LOG_TERMINAL) << "Or visit us on IRC at irc.anope.org #anope";
        throw CoreException();
    }

    if (GetCommandLineArgument("nofork", 'n')) {
        Anope::NoFork = true;
    }

    if (GetCommandLineArgument("support", 's')) {
        Anope::NoFork = Anope::NoThird = true;
        ++Anope::Debug;
    }

    if (GetCommandLineArgument("readonly", 'r')) {
        Anope::ReadOnly = true;
    }

    if (GetCommandLineArgument("nothird")) {
        Anope::NoThird = true;
    }

    if (GetCommandLineArgument("noexpire", 'e')) {
        Anope::NoExpire = true;
    }

    if (GetCommandLineArgument("protocoldebug")) {
        Anope::ProtocolDebug = true;
    }

    Anope::string arg;
    if (GetCommandLineArgument("debug", 'd', arg)) {
        if (!arg.empty()) {
            int level = arg.is_number_only() ? convertTo<int>(arg) : -1;
            if (level > 0) {
                Anope::Debug = level;
            } else {
                throw CoreException("Invalid option given to --debug");
            }
        } else {
            ++Anope::Debug;
        }
    }

    if (GetCommandLineArgument("config", 'c', arg)) {
        if (arg.empty()) {
            throw CoreException("The --config option requires a file name");
        }
        ServicesConf = Configuration::File(arg, false);
    }

    if (GetCommandLineArgument("confdir", 0, arg)) {
        if (arg.empty()) {
            throw CoreException("The --confdir option requires a path");
        }
        Anope::ConfigDir = arg;
    }

    if (GetCommandLineArgument("dbdir", 0, arg)) {
        if (arg.empty()) {
            throw CoreException("The --dbdir option requires a path");
        }
        Anope::DataDir = arg;
    }

    if (GetCommandLineArgument("localedir", 0, arg)) {
        if (arg.empty()) {
            throw CoreException("The --localedir option requires a path");
        }
        Anope::LocaleDir = arg;
    }

    if (GetCommandLineArgument("modulesdir", 0, arg)) {
        if (arg.empty()) {
            throw CoreException("The --modulesdir option requires a path");
        }
        Anope::ModuleDir = arg;
    }

    if (GetCommandLineArgument("logdir", 0, arg)) {
        if (arg.empty()) {
            throw CoreException("The --logdir option requires a path");
        }
        Anope::LogDir = arg;
    }

    /* Chdir to Services data directory. */
    if (chdir(Anope::ServicesDir.c_str()) < 0) {
        throw CoreException("Unable to chdir to " + Anope::ServicesDir + ": " +
                            Anope::LastError());
    }

    Log(LOG_TERMINAL) << "Anope " << Anope::Version() << ", " <<
                      Anope::VersionBuildString();

#ifdef _WIN32
    if (!SupportedWindowsVersion()) {
        throw CoreException(GetWindowsVersion() +
                            " is not a supported version of Windows");
    }
#else
    /* If we're root, issue a warning now */
    if (!getuid() && !getgid()) {
        /* If we are configured to setuid later, don't issue a warning */
        Configuration::Block *options = Config->GetBlock("options");
        if (options->Get<const Anope::string>("user").empty()) {
            std::cerr <<
                      "WARNING: You are currently running Anope as the root superuser. Anope does not"
                      << std::endl;
            std::cerr <<
                      "         require root privileges to run, and it is discouraged that you run Anope"
                      << std::endl;
            std::cerr << "         as the root superuser." << std::endl;
            sleep(3);
        }
    }
#endif

#ifdef _WIN32
    Log(LOG_TERMINAL) << "Using configuration file " << Anope::ConfigDir << "\\" <<
                      ServicesConf.GetName();
#else
    Log(LOG_TERMINAL) << "Using configuration file " << Anope::ConfigDir << "/" <<
                      ServicesConf.GetName();

    /* Fork to background */
    if (!Anope::NoFork) {
        /* Install these before fork() - it is possible for the child to
         * connect and kill() the parent before it is able to install the
         * handler.
         */
        struct sigaction sa, old_sigusr2, old_sigchld;

        sa.sa_flags = 0;
        sigemptyset(&sa.sa_mask);
        sa.sa_handler = parent_signal_handler;

        sigaction(SIGUSR2, &sa, &old_sigusr2);
        sigaction(SIGCHLD, &sa, &old_sigchld);

        int i = fork();
        if (i > 0) {
            sigset_t mask;

            sigemptyset(&mask);
            sigsuspend(&mask);

            exit(Anope::ReturnValue);
        } else if (i == -1) {
            Log() << "Error, unable to fork: " << Anope::LastError();
            Anope::NoFork = true;
        }

        /* Child doesn't need these */
        sigaction(SIGUSR2, &old_sigusr2, NULL);
        sigaction(SIGCHLD, &old_sigchld, NULL);
    }

#endif

    /* Initialize the socket engine. Note that some engines can not survive a fork(), so this must be here. */
    SocketEngine::Init();

    /* Read configuration file; exit if there are problems. */
    try {
        Config = new Configuration::Conf();
    } catch (const ConfigException &ex) {
        Log(LOG_TERMINAL) << ex.GetReason();
        Log(LOG_TERMINAL) <<
                          "*** Support resources: Read through the services.conf self-contained";
        Log(LOG_TERMINAL) <<
                          "*** documentation. Read the documentation files found in the 'docs'";
        Log(LOG_TERMINAL) <<
                          "*** folder. Visit our portal located at https://www.anope.org/. Join";
        Log(LOG_TERMINAL) <<
                          "*** our support channel on /server irc.anope.org channel #anope.";
        throw CoreException("Configuration file failed to validate");
    }

    /* Create me */
    Configuration::Block *block = Config->GetBlock("serverinfo");
    Me = new Server(NULL, block->Get<const Anope::string>("name"), 0,
                    block->Get<const Anope::string>("description"),
                    block->Get<const Anope::string>("id"));
    for (botinfo_map::const_iterator it = BotListByNick->begin(),
            it_end = BotListByNick->end(); it != it_end; ++it) {
        it->second->server = Me;
        ++Me->users;
    }

    /* Announce ourselves to the logfile. */
    Log() << "Anope " << Anope::Version() << " starting up" << (Anope::Debug
            || Anope::ReadOnly ? " (options:" : "") << (Anope::Debug ? " debug" : "") <<
          (Anope::ReadOnly ? " readonly" : "") << (Anope::Debug
                  || Anope::ReadOnly ? ")" : "");

    InitSignals();

    /* Initialize multi-language support */
    Language::InitLanguages();

    /* Initialize random number generator */
    block = Config->GetBlock("options");
    srand(block->Get<unsigned>("seed") ^ time(NULL));

    /* load modules */
    Log() << "Loading modules...";
    for (int i = 0; i < Config->CountBlock("module"); ++i) {
        ModuleManager::LoadModule(Config->GetBlock("module",
                                  i)->Get<const Anope::string>("name"), NULL);
    }

#ifndef _WIN32
    /* We won't background later, so we should setuid now */
    if (Anope::NoFork) {
        setuidgid();
    }
#endif

    Module *protocol = ModuleManager::FindFirstOf(PROTOCOL);
    if (protocol == NULL) {
        throw CoreException("You must load a protocol module!");
    }

    /* Write our PID to the PID file. */
    write_pidfile();

    Log() << "Using IRCd protocol " << protocol->name;

    /* Auto assign sid if applicable */
    if (IRCD->RequiresID) {
        Anope::string sid = IRCD->SID_Retrieve();
        if (Me->GetSID() == Me->GetName()) {
            Me->SetSID(sid);
        }
        for (botinfo_map::iterator it = BotListByNick->begin(),
                it_end = BotListByNick->end(); it != it_end; ++it) {
            it->second->GenerateUID();
        }
    }

    /* Load up databases */
    Log() << "Loading databases...";
    EventReturn MOD_RESULT;
    FOREACH_RESULT(OnLoadDatabase, MOD_RESULT, ());
    static_cast<void>(MOD_RESULT);
    Log() << "Databases loaded";

    FOREACH_MOD(OnPostInit, ());

    for (channel_map::const_iterator it = ChannelList.begin(),
            it_end = ChannelList.end(); it != it_end; ++it) {
        it->second->Sync();
    }

    Serialize::CheckTypes();
}
