/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2005 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
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
#include "inspircd_util.h"
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
#include "typedefs.h"

extern int MODCOUNT;
extern struct sockaddr_in client,server;
extern socklen_t length;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
std::vector<InspSocket*> module_sockets;
extern time_t TIME;
extern time_t OLDTIME;

extern InspIRCd* ServerInstance;
extern SocketEngine* SE;
extern serverstats* stats;
extern ServerConfig *Config;

extern userrec* fd_ref_table[65536];
char data[65536];

extern user_hash clientlist;
extern chan_hash chanlist;

void ProcessUser(userrec* cu)
{
        int result = EAGAIN;
        log(DEBUG,"Processing user with fd %d",cu->fd);
        int MOD_RESULT = 0;
        int result2 = 0;
        FOREACH_RESULT(OnRawSocketRead(cu->fd,data,65535,result2));
        if (!MOD_RESULT)
        {
                result = cu->ReadData(data, 65535);
        }
        else
        {
                log(DEBUG,"Data result returned by module: %d",MOD_RESULT);
                result = result2;
        }
        log(DEBUG,"Read result: %d",result);
        if (result)
        {
                stats->statsRecv += result;
                // perform a check on the raw buffer as an array (not a string!) to remove
                // characters 0 and 7 which are illegal in the RFC - replace them with spaces.
                // hopefully this should stop even more people whining about "Unknown command: *"
                for (int checker = 0; checker < result; checker++)
                {
                        if ((data[checker] == 0) || (data[checker] == 7))
                                data[checker] = ' ';
                }
                if (result > 0)
                        data[result] = '\0';
                userrec* current = cu;
                int currfd = current->fd;
                int floodlines = 0;
                // add the data to the users buffer
                if (result > 0)
                {
                        if (!current->AddBuffer(data))
                        {
                                // AddBuffer returned false, theres too much data in the user's buffer and theyre up to no good.
                                if (current->registered == 7)
                                {
                                        kill_link(current,"RecvQ exceeded");
                                }
                                else
                                {
                                        WriteOpers("*** Excess flood from %s",current->ip);
                                        log(DEFAULT,"Excess flood from: %s",current->ip);
                                        add_zline(120,Config->ServerName,"Flood from unregistered connection",current->ip);
                                        apply_lines(APPLY_ZLINES);
                                }
                                return;
                        }
                        if (current->recvq.length() > (unsigned)Config->NetBufferSize)
                        {
                                if (current->registered == 7)
                                {
                                        kill_link(current,"RecvQ exceeded");
                                }
                                else
                                {
                                        WriteOpers("*** Excess flood from %s",current->ip);
                                        log(DEFAULT,"Excess flood from: %s",current->ip);
                                        add_zline(120,Config->ServerName,"Flood from unregistered connection",current->ip);
                                        apply_lines(APPLY_ZLINES);
                                }
                                return;
                        }
                        // while there are complete lines to process...
                        while (current->BufferIsReady())
                        {
                                floodlines++;
                                if (TIME > current->reset_due)
                                {
                                        current->reset_due = TIME + current->threshold;
                                        current->lines_in = 0;
                                }
                                current->lines_in++;
                                if (current->lines_in > current->flood)
                                {
                                        log(DEFAULT,"Excess flood from: %s!%s@%s",current->nick,current->ident,current->host);
                                        WriteOpers("*** Excess flood from: %s!%s@%s",current->nick,current->ident,current->host);
                                        kill_link(current,"Excess flood");
                                        return;
                                }
                                if ((floodlines > current->flood) && (current->flood != 0))
                                {
                                        if (current->registered == 7)
                                        {
                                                log(DEFAULT,"Excess flood from: %s!%s@%s",current->nick,current->ident,current->host);
                                                WriteOpers("*** Excess flood from: %s!%s@%s",current->nick,current->ident,current->host);
                                                kill_link(current,"Excess flood");
                                        }
                                        else
                                        {
                                                add_zline(120,Config->ServerName,"Flood from unregistered connection",current->ip);
                                                apply_lines(APPLY_ZLINES);
                                        }
                                        return;
                                }
                                char sanitized[MAXBUF];
                                // use GetBuffer to copy single lines into the sanitized string
                                std::string single_line = current->GetBuffer();
                                current->bytes_in += single_line.length();
                                current->cmds_in++;
                                if (single_line.length()>512)
                                {
                                        log(DEFAULT,"Excess flood from: %s!%s@%s",current->nick,current->ident,current->host);
                                        WriteOpers("*** Excess flood from: %s!%s@%s",current->nick,current->ident,current->host);
                                        kill_link(current,"Excess flood");
                                        return;
                                }
                                strlcpy(sanitized,single_line.c_str(),MAXBUF);
                                if (*sanitized)
                                {
                                        userrec* old_comp = fd_ref_table[currfd];
                                        // we're gonna re-scan to check if the nick is gone, after every
                                        // command - if it has, we're gonna bail
                                        process_buffer(sanitized,current);
                                        // look for the user's record in case it's changed... if theyve quit,
                                        // we cant do anything more with their buffer, so bail.
                                        // there used to be an ugly, slow loop here. Now we have a reference
                                        // table, life is much easier (and FASTER)
                                        userrec* new_comp = fd_ref_table[currfd];
                                        if ((currfd < 0) || (!fd_ref_table[currfd]) || (old_comp != new_comp))
                                        {
                                                return;
                                        }
                                        else
                                        {
                                                /* The user is still here, flush their buffer */
                                                current->FlushWriteBuf();
                                        }
                                }
                        }
                        return;
                }
                if ((result == -1) && (errno != EAGAIN) && (errno != EINTR))
                {
                        log(DEBUG,"killing: %s",cu->nick);
                        kill_link(cu,strerror(errno));
                        return;
                }
        }
        // result EAGAIN means nothing read
        else if (result == EAGAIN)
        {
        }
        else if (result == 0)
        {
                log(DEBUG,"InspIRCd: Exited: %s",cu->nick);
                kill_link(cu,"Client exited");
                log(DEBUG,"Bailing from client exit");
                return;
        }
}


/**
 * This function is called once a second from the mainloop.
 * It is intended to do background checking on all the user structs, e.g.
 * stuff like ping checks, registration timeouts, etc.
 * The function returns false when it is finished, and true if
 * it needs to be run again (e.g. it has processed one of a batch of
 * QUIT messages, but couldnt continue iterating because the iterator
 * became invalid). This function is also responsible for checking
 * if InspSocket derived classes are timed out.
 */
bool DoBackgroundUserStuff(time_t TIME)
{
        unsigned int numsockets = ServerInstance->module_sockets.size();
        for (std::vector<InspSocket*>::iterator a = ServerInstance->module_sockets.begin(); a < ServerInstance->module_sockets.end(); a++)
        {
                InspSocket* s = (InspSocket*)*a;
                if (s->Timeout(TIME))
                {
                        log(DEBUG,"Socket poll returned false, close and bail");
                        SE->DelFd(s->GetFd());
                        s->Close();
                        ServerInstance->module_sockets.erase(a);
                        delete s;
                        break;
                }
                if (ServerInstance->module_sockets.size() != numsockets) break;
        }
        /* TODO: We need a seperate hash containing only local users for this
         */
        for (user_hash::iterator count2 = clientlist.begin(); count2 != clientlist.end(); count2++)
        {
                /* Sanity checks for corrupted iterators (yes, really) */
                userrec* curr = NULL;
                if (count2->second)
                        curr = count2->second;
                if ((long)curr == -1)
                        return false;

                if ((curr) && (curr->fd != 0)) /* XXX - why are we checking fd != 0? --w00t */
                {
                        int currfd = curr->fd;
                        // we don't check the state of remote users.
			if (IS_LOCAL(curr))
                        {
                                curr->FlushWriteBuf();
                                if (curr->GetWriteError() != "")
                                {
                                        log(DEBUG,"InspIRCd: write error: %s",curr->GetWriteError().c_str());
                                        kill_link(curr,curr->GetWriteError().c_str());
                                        return true;
                                }
                                // registration timeout -- didnt send USER/NICK/HOST in the time specified in
                                // their connection class.
                                if (((unsigned)TIME > (unsigned)curr->timeout) && (curr->registered != 7))
                                {
                                        log(DEBUG,"InspIRCd: registration timeout: %s",curr->nick);
                                        kill_link(curr,"Registration timeout");
                                        return true;
                                }
                                if ((TIME > curr->signon) && (curr->registered == 3) && (AllModulesReportReady(curr)))
                                {
                                        log(DEBUG,"signon exceed, registered=3, and modules ready, OK: %d %d",TIME,curr->signon);
                                        curr->dns_done = true;
                                        stats->statsDnsBad++;
                                        FullConnectUser(curr);
                                        if (fd_ref_table[currfd] != curr) // something changed, bail pronto
                                                return true;
                                 }
                                 if ((curr->dns_done) && (curr->registered == 3) && (AllModulesReportReady(curr)))
                                 {
                                       log(DEBUG,"dns done, registered=3, and modules ready, OK");
                                       FullConnectUser(curr);
                                       if (fd_ref_table[currfd] != curr) // something changed, bail pronto
                                                return true;
                                 }
                                 if ((TIME > curr->nping) && (isnick(curr->nick)) && (curr->registered == 7))
                                 {
                                       if ((!curr->lastping) && (curr->registered == 7))
                                       {
                                               log(DEBUG,"InspIRCd: ping timeout: %s",curr->nick);
                                               kill_link(curr,"Ping timeout");
                                               return true;
                                       }
                                       Write(curr->fd,"PING :%s",Config->ServerName);
                                       log(DEBUG,"InspIRCd: pinging: %s",curr->nick);
                                       curr->lastping = 0;
                                       curr->nping = TIME+curr->pingmax;       // was hard coded to 120
                                }
                        }
                }
        }
        return false;
}

void OpenLog(char** argv, int argc)
{
        std::string logpath = GetFullProgDir(argv,argc) + "/ircd.log";
        Config->log_file = fopen(logpath.c_str(),"a+");
        if (!Config->log_file)
        {
                printf("ERROR: Could not write to logfile %s, bailing!\n\n",logpath.c_str());
                Exit(ERROR);
        }
#ifdef IS_CYGWIN
        printf("Logging to ircd.log...\n");
#else
        printf("Logging to %s...\n",logpath.c_str());
#endif
}


void CheckRoot()
{
        if (geteuid() == 0)
        {
                printf("WARNING!!! You are running an irc server as ROOT!!! DO NOT DO THIS!!!\n\n");
                log(DEFAULT,"InspIRCd: startup: not starting with UID 0!");
                Exit(ERROR);
        }
}


void CheckDie()
{
        if (*Config->DieValue)
        {
                printf("WARNING: %s\n\n",Config->DieValue);
                log(DEFAULT,"Ut-Oh, somebody didn't read their config file: '%s'",Config->DieValue);
                exit(0);
        }
}

void LoadAllModules()
{
        /* We must load the modules AFTER initializing the socket engine, now */
        MODCOUNT = -1;
	char configToken[MAXBUF];
        for (int count = 0; count < Config->ConfValueEnum("module",&Config->config_f); count++)
        {
                Config->ConfValue("module","name",count,configToken,&Config->config_f);
                printf("Loading module... \033[1;32m%s\033[0m\n",configToken);
                if (!ServerInstance->LoadModule(configToken))
                {
                        log(DEFAULT,"Exiting due to a module loader error.");
                        printf("\nThere was an error loading a module: %s\n\nYou might want to do './inspircd start' instead of 'bin/inspircd'\n\n",ModuleError());
                        Exit(0);
                }
        }
        log(DEFAULT,"Total loaded modules: %lu",(unsigned long)MODCOUNT+1);
}


