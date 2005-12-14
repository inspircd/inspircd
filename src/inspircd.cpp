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
#include "userprocess.h"
#include "socket.h"
#include "dns.h"
#include "typedefs.h"

InspIRCd* ServerInstance;

int WHOWAS_STALE = 48; // default WHOWAS Entries last 2 days before they go 'stale'
int WHOWAS_MAX = 100;  // default 100 people maximum in the WHOWAS list

extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
std::vector<InspSocket*> module_sockets;

extern int MODCOUNT;
int openSockfd[MAXSOCKS];
sockaddr_in client,server;
socklen_t length;

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
int BoundPortCount = 0;
std::vector<userrec*> all_opers;
char lowermap[255];


void AddOper(userrec* user)
{
	log(DEBUG,"Oper added to optimization list");
	all_opers.push_back(user);
}

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

void DeleteOper(userrec* user)
{
        for (std::vector<userrec*>::iterator a = all_opers.begin(); a < all_opers.end(); a++)
        {
                if (*a == user)
                {
                        log(DEBUG,"Oper removed from optimization list");
                        all_opers.erase(a);
                        return;
                }
        }
}

std::string GetRevision()
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


/* This function pokes and hacks at a parameter list like the following:
 *
 * PART #winbot,#darkgalaxy :m00!
 *
 * to turn it into a series of individual calls like this:
 *
 * PART #winbot :m00!
 * PART #darkgalaxy :m00!
 *
 * The seperate calls are sent to a callback function provided by the caller
 * (the caller will usually call itself recursively). The callback function
 * must be a command handler. Calling this function on a line with no list causes
 * no action to be taken. You must provide a starting and ending parameter number
 * where the range of the list can be found, useful if you have a terminating
 * parameter as above which is actually not part of the list, or parameters
 * before the actual list as well. This code is used by many functions which
 * can function as "one to list" (see the RFC) */

int loop_call(handlerfunc fn, char **parameters, int pcnt, userrec *u, int start, int end, int joins)
{
	char plist[MAXBUF];
	char *param;
	char *pars[32];
	char blog[32][MAXBUF];
	char blog2[32][MAXBUF];
	int j = 0, q = 0, total = 0, t = 0, t2 = 0, total2 = 0;
	char keystr[MAXBUF];
	char moo[MAXBUF];

	for (int i = 0; i <32; i++)
		strcpy(blog[i],"");

	for (int i = 0; i <32; i++)
		strcpy(blog2[i],"");

	strcpy(moo,"");
	for (int i = 0; i <10; i++)
	{
		if (!parameters[i])
		{
			parameters[i] = moo;
		}
	}
	if (joins)
	{
		if (pcnt > 1) /* we have a key to copy */
		{
			strlcpy(keystr,parameters[1],MAXBUF);
		}
	}

	if (!parameters[start])
	{
		return 0;
	}
	if (!strchr(parameters[start],','))
	{
		return 0;
	}
	strcpy(plist,"");
	for (int i = start; i <= end; i++)
	{
		if (parameters[i])
		{
			strlcat(plist,parameters[i],MAXBUF);
		}
	}
	
	j = 0;
	param = plist;

	t = strlen(plist);
	for (int i = 0; i < t; i++)
	{
		if (plist[i] == ',')
		{
			plist[i] = '\0';
			strlcpy(blog[j++],param,MAXBUF);
			param = plist+i+1;
			if (j>20)
			{
				WriteServ(u->fd,"407 %s %s :Too many targets in list, message not delivered.",u->nick,blog[j-1]);
				return 1;
			}
		}
	}
	strlcpy(blog[j++],param,MAXBUF);
	total = j;

	if ((joins) && (keystr) && (total>0)) // more than one channel and is joining
	{
		strcat(keystr,",");
	}
	
	if ((joins) && (keystr))
	{
		if (strchr(keystr,','))
		{
			j = 0;
			param = keystr;
			t2 = strlen(keystr);
			for (int i = 0; i < t2; i++)
			{
				if (keystr[i] == ',')
				{
					keystr[i] = '\0';
					strlcpy(blog2[j++],param,MAXBUF);
					param = keystr+i+1;
				}
			}
			strlcpy(blog2[j++],param,MAXBUF);
			total2 = j;
		}
	}

	for (j = 0; j < total; j++)
	{
		if (blog[j])
		{
			pars[0] = blog[j];
		}
		for (q = end; q < pcnt-1; q++)
		{
			if (parameters[q+1])
			{
				pars[q-end+1] = parameters[q+1];
			}
		}
		if ((joins) && (parameters[1]))
		{
			if (pcnt > 1)
			{
				pars[1] = blog2[j];
			}
			else
			{
				pars[1] = NULL;
			}
		}
		/* repeatedly call the function with the hacked parameter list */
		if ((joins) && (pcnt > 1))
		{
			if (pars[1])
			{
				// pars[1] already set up and containing key from blog2[j]
				fn(pars,2,u);
			}
			else
			{
				pars[1] = parameters[1];
				fn(pars,2,u);
			}
		}
		else
		{
			fn(pars,pcnt-(end-start),u);
		}
	}

	return 1;
}



void kill_link(userrec *user,const char* r)
{
	user_hash::iterator iter = clientlist.find(user->nick);
	
	char reason[MAXBUF];
	
	strncpy(reason,r,MAXBUF);

	if (strlen(reason)>MAXQUIT)
	{
		reason[MAXQUIT-1] = '\0';
	}

	log(DEBUG,"kill_link: %s '%s'",user->nick,reason);
	Write(user->fd,"ERROR :Closing link (%s@%s) [%s]",user->ident,user->host,reason);
	log(DEBUG,"closing fd %lu",(unsigned long)user->fd);

	if (user->registered == 7) {
		FOREACH_MOD OnUserQuit(user,reason);
		WriteCommonExcept(user,"QUIT :%s",reason);
	}

	user->FlushWriteBuf();

	FOREACH_MOD OnUserDisconnect(user);

	if (user->fd > -1)
	{
		FOREACH_MOD OnRawSocketClose(user->fd);
		SE->DelFd(user->fd);
		user->CloseSocket();
	}

	// this must come before the WriteOpers so that it doesnt try to fill their buffer with anything
	// if they were an oper with +s.
        if (user->registered == 7) {
                purge_empty_chans(user);
		// fix by brain: only show local quits because we only show local connects (it just makes SENSE)
		if (user->fd > -1)
			WriteOpers("*** Client exiting: %s!%s@%s [%s]",user->nick,user->ident,user->host,reason);
		AddWhoWas(user);
	}

	if (iter != clientlist.end())
	{
		log(DEBUG,"deleting user hash value %lu",(unsigned long)user);
		if (user->fd > -1)
			fd_ref_table[user->fd] = NULL;
		clientlist.erase(iter);
	}
	delete user;
}

void kill_link_silent(userrec *user,const char* r)
{
	user_hash::iterator iter = clientlist.find(user->nick);
	
	char reason[MAXBUF];
	
	strncpy(reason,r,MAXBUF);

	if (strlen(reason)>MAXQUIT)
	{
		reason[MAXQUIT-1] = '\0';
	}

	log(DEBUG,"kill_link: %s '%s'",user->nick,reason);
	Write(user->fd,"ERROR :Closing link (%s@%s) [%s]",user->ident,user->host,reason);
	log(DEBUG,"closing fd %lu",(unsigned long)user->fd);

	user->FlushWriteBuf();

	if (user->registered == 7) {
		FOREACH_MOD OnUserQuit(user,reason);
		WriteCommonExcept(user,"QUIT :%s",reason);
	}

	FOREACH_MOD OnUserDisconnect(user);

        if (user->fd > -1)
        {
		FOREACH_MOD OnRawSocketClose(user->fd);
		SE->DelFd(user->fd);
		user->CloseSocket();
        }

        if (user->registered == 7) {
                purge_empty_chans(user);
        }
	
	if (iter != clientlist.end())
	{
		log(DEBUG,"deleting user hash value %lu",(unsigned long)user);
                if (user->fd > -1)
                        fd_ref_table[user->fd] = NULL;
		clientlist.erase(iter);
	}
	delete user;
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
        BoundPortCount = BindPorts();

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

template<typename T> inline string ConvToStr(const T &in)
{
	stringstream tmp;
	if (!(tmp << in)) return string();
	return tmp.str();
}

/* re-allocates a nick in the user_hash after they change nicknames,
 * returns a pointer to the new user as it may have moved */

userrec* ReHashNick(char* Old, char* New)
{
	//user_hash::iterator newnick;
	user_hash::iterator oldnick = clientlist.find(Old);

	log(DEBUG,"ReHashNick: %s %s",Old,New);
	
	if (!strcasecmp(Old,New))
	{
		log(DEBUG,"old nick is new nick, skipping");
		return oldnick->second;
	}
	
	if (oldnick == clientlist.end()) return NULL; /* doesnt exist */

	log(DEBUG,"ReHashNick: Found hashed nick %s",Old);

	userrec* olduser = oldnick->second;
	clientlist[New] = olduser;
	clientlist.erase(oldnick);

	log(DEBUG,"ReHashNick: Nick rehashed as %s",New);
	
	return clientlist[New];
}

/* adds or updates an entry in the whowas list */
void AddWhoWas(userrec* u)
{
	whowas_hash::iterator iter = whowas.find(u->nick);
	WhoWasUser *a = new WhoWasUser();
	strlcpy(a->nick,u->nick,NICKMAX);
	strlcpy(a->ident,u->ident,IDENTMAX);
	strlcpy(a->dhost,u->dhost,160);
	strlcpy(a->host,u->host,160);
	strlcpy(a->fullname,u->fullname,MAXGECOS);
	strlcpy(a->server,u->server,256);
	a->signon = u->signon;

	/* MAX_WHOWAS:   max number of /WHOWAS items
	 * WHOWAS_STALE: number of hours before a WHOWAS item is marked as stale and
	 *		 can be replaced by a newer one
	 */
	
	if (iter == whowas.end())
	{
		if (whowas.size() >= (unsigned)WHOWAS_MAX)
		{
			for (whowas_hash::iterator i = whowas.begin(); i != whowas.end(); i++)
			{
				// 3600 seconds in an hour ;)
				if ((i->second->signon)<(TIME-(WHOWAS_STALE*3600)))
				{
					// delete the old one
					if (i->second) delete i->second;
					// replace with new one
					i->second = a;
					log(DEBUG,"added WHOWAS entry, purged an old record");
					return;
				}
			}
			// no space left and user doesnt exist. Don't leave ram in use!
			log(DEBUG,"Not able to update whowas (list at WHOWAS_MAX entries and trying to add new?), freeing excess ram");
			delete a;
		}
		else
		{
			log(DEBUG,"added fresh WHOWAS entry");
			whowas[a->nick] = a;
		}
	}
	else
	{
		log(DEBUG,"updated WHOWAS entry");
		if (iter->second) delete iter->second;
		iter->second = a;
	}
}

#ifdef THREADED_DNS
void* dns_task(void* arg)
{
	userrec* u = (userrec*)arg;
	log(DEBUG,"DNS thread for user %s",u->nick);
	DNS dns1;
	DNS dns2;
	std::string host;
	std::string ip;
	if (dns1.ReverseLookup(u->ip))
	{
		log(DEBUG,"DNS Step 1");
		while (!dns1.HasResult())
		{
			usleep(100);
		}
		host = dns1.GetResult();
		if (host != "")
		{
			log(DEBUG,"DNS Step 2: '%s'",host.c_str());
			if (dns2.ForwardLookup(host))
			{
				while (!dns2.HasResult())
				{
					usleep(100);
				}
				ip = dns2.GetResultIP();
				log(DEBUG,"DNS Step 3 '%s'(%d) '%s'(%d)",ip.c_str(),ip.length(),u->ip,strlen(u->ip));
				if (ip == std::string(u->ip))
				{
					log(DEBUG,"DNS Step 4");
					if (host.length() < 160)
					{
						log(DEBUG,"DNS Step 5");
						strcpy(u->host,host.c_str());
						strcpy(u->dhost,host.c_str());
					}
				}
			}
		}
	}
	u->dns_done = true;
	return NULL;
}
#endif

/* add a client connection to the sockets list */
void AddClient(int socket, char* host, int port, bool iscached, char* ip)
{
	string tempnick;
	char tn2[MAXBUF];
	user_hash::iterator iter;

	tempnick = ConvToStr(socket) + "-unknown";
	sprintf(tn2,"%lu-unknown",(unsigned long)socket);

	iter = clientlist.find(tempnick);

	// fix by brain.
	// as these nicknames are 'RFC impossible', we can be sure nobody is going to be
	// using one as a registered connection. As theyre per fd, we can also safely assume
	// that we wont have collisions. Therefore, if the nick exists in the list, its only
	// used by a dead socket, erase the iterator so that the new client may reclaim it.
	// this was probably the cause of 'server ignores me when i hammer it with reconnects'
	// issue in earlier alphas/betas
	if (iter != clientlist.end())
	{
		userrec* goner = iter->second;
		delete goner;
		clientlist.erase(iter);
	}

	/*
	 * It is OK to access the value here this way since we know
	 * it exists, we just created it above.
	 *
	 * At NO other time should you access a value in a map or a
	 * hash_map this way.
	 */
	clientlist[tempnick] = new userrec();

	NonBlocking(socket);
	log(DEBUG,"AddClient: %lu %s %d %s",(unsigned long)socket,host,port,ip);

	clientlist[tempnick]->fd = socket;
	strlcpy(clientlist[tempnick]->nick, tn2,NICKMAX);
	strlcpy(clientlist[tempnick]->host, host,160);
	strlcpy(clientlist[tempnick]->dhost, host,160);
	clientlist[tempnick]->server = (char*)FindServerNamePtr(Config->ServerName);
	strlcpy(clientlist[tempnick]->ident, "unknown",IDENTMAX);
	clientlist[tempnick]->registered = 0;
	clientlist[tempnick]->signon = TIME + Config->dns_timeout;
	clientlist[tempnick]->lastping = 1;
	clientlist[tempnick]->port = port;
	strlcpy(clientlist[tempnick]->ip,ip,16);

	// set the registration timeout for this user
	unsigned long class_regtimeout = 90;
	int class_flood = 0;
	long class_threshold = 5;
	long class_sqmax = 262144;	// 256kb
	long class_rqmax = 4096;	// 4k

	for (ClassVector::iterator i = Config->Classes.begin(); i != Config->Classes.end(); i++)
	{
		if (match(clientlist[tempnick]->host,i->host) && (i->type == CC_ALLOW))
		{
			class_regtimeout = (unsigned long)i->registration_timeout;
			class_flood = i->flood;
			clientlist[tempnick]->pingmax = i->pingtime;
			class_threshold = i->threshold;
			class_sqmax = i->sendqmax;
			class_rqmax = i->recvqmax;
			break;
		}
	}

	clientlist[tempnick]->nping = TIME+clientlist[tempnick]->pingmax + Config->dns_timeout;
	clientlist[tempnick]->timeout = TIME+class_regtimeout;
	clientlist[tempnick]->flood = class_flood;
	clientlist[tempnick]->threshold = class_threshold;
	clientlist[tempnick]->sendqmax = class_sqmax;
	clientlist[tempnick]->recvqmax = class_rqmax;

	ucrec a;
	a.channel = NULL;
	a.uc_modes = 0;
	for (int i = 0; i < MAXCHANS; i++)
		clientlist[tempnick]->chans.push_back(a);

	if (clientlist.size() > Config->SoftLimit)
	{
		kill_link(clientlist[tempnick],"No more connections allowed");
		return;
	}

	if (clientlist.size() >= MAXCLIENTS)
	{
		kill_link(clientlist[tempnick],"No more connections allowed");
		return;
	}

	// this is done as a safety check to keep the file descriptors within range of fd_ref_table.
	// its a pretty big but for the moment valid assumption:
	// file descriptors are handed out starting at 0, and are recycled as theyre freed.
	// therefore if there is ever an fd over 65535, 65536 clients must be connected to the
	// irc server at once (or the irc server otherwise initiating this many connections, files etc)
	// which for the time being is a physical impossibility (even the largest networks dont have more
	// than about 10,000 users on ONE server!)
	if ((unsigned)socket > 65534)
	{
		kill_link(clientlist[tempnick],"Server is full");
		return;
	}
		

        char* e = matches_exception(ip);
	if (!e)
	{
		char* r = matches_zline(ip);
		if (r)
		{
			char reason[MAXBUF];
			snprintf(reason,MAXBUF,"Z-Lined: %s",r);
			kill_link(clientlist[tempnick],reason);
			return;
		}
	}
	fd_ref_table[socket] = clientlist[tempnick];
	SE->AddFd(socket,true,X_ESTAB_CLIENT);
}

/* shows the message of the day, and any other on-logon stuff */
void FullConnectUser(userrec* user)
{
	stats->statsConnects++;
        user->idle_lastmsg = TIME;
        log(DEBUG,"ConnectUser: %s",user->nick);

        if ((strcmp(Passwd(user),"")) && (!user->haspassed))
        {
                kill_link(user,"Invalid password");
                return;
        }
        if (IsDenied(user))
        {
                kill_link(user,"Unauthorised connection");
                return;
        }

        char match_against[MAXBUF];
        snprintf(match_against,MAXBUF,"%s@%s",user->ident,user->host);
	char* e = matches_exception(match_against);
	if (!e)
	{
        	char* r = matches_gline(match_against);
        	if (r)
        	{
                	char reason[MAXBUF];
                	snprintf(reason,MAXBUF,"G-Lined: %s",r);
                	kill_link_silent(user,reason);
                	return;
        	}
        	r = matches_kline(user->host);
        	if (r)
        	{
                	char reason[MAXBUF];
                	snprintf(reason,MAXBUF,"K-Lined: %s",r);
                	kill_link_silent(user,reason);
                	return;
	        }
	}


        WriteServ(user->fd,"NOTICE Auth :Welcome to \002%s\002!",Config->Network);
        WriteServ(user->fd,"001 %s :Welcome to the %s IRC Network %s!%s@%s",user->nick,Config->Network,user->nick,user->ident,user->host);
        WriteServ(user->fd,"002 %s :Your host is %s, running version %s",user->nick,Config->ServerName,VERSION);
        WriteServ(user->fd,"003 %s :This server was created %s %s",user->nick,__TIME__,__DATE__);
        WriteServ(user->fd,"004 %s %s %s iowghraAsORVSxNCWqBzvdHtGI lvhopsmntikrRcaqOALQbSeKVfHGCuzN",user->nick,Config->ServerName,VERSION);
        // the neatest way to construct the initial 005 numeric, considering the number of configure constants to go in it...
        std::stringstream v;
        v << "WALLCHOPS MODES=13 CHANTYPES=# PREFIX=(ohv)@%+ MAP SAFELIST MAXCHANNELS=" << MAXCHANS;
        v << " MAXBANS=60 NICKLEN=" << NICKMAX;
        v << " TOPICLEN=" << MAXTOPIC << " KICKLEN=" << MAXKICK << " MAXTARGETS=20 AWAYLEN=" << MAXAWAY << " CHANMODES=ohvb,k,l,psmnti NETWORK=";
        v << Config->Network;
        std::string data005 = v.str();
        FOREACH_MOD On005Numeric(data005);
        // anfl @ #ratbox, efnet reminded me that according to the RFC this cant contain more than 13 tokens per line...
        // so i'd better split it :)
        std::stringstream out(data005);
        std::string token = "";
        std::string line5 = "";
        int token_counter = 0;
        while (!out.eof())
        {
                out >> token;
                line5 = line5 + token + " ";
                token_counter++;
                if ((token_counter >= 13) || (out.eof() == true))
                {
                        WriteServ(user->fd,"005 %s %s:are supported by this server",user->nick,line5.c_str());
                        line5 = "";
                        token_counter = 0;
                }
        }
        ShowMOTD(user);

	// fix 3 by brain, move registered = 7 below these so that spurious modes and host changes dont go out
	// onto the network and produce 'fake direction'
	FOREACH_MOD OnUserConnect(user);
	FOREACH_MOD OnGlobalConnect(user);
	user->registered = 7;
	WriteOpers("*** Client connecting on port %lu: %s!%s@%s [%s]",(unsigned long)user->port,user->nick,user->ident,user->host,user->ip);
}


/* shows the message of the day, and any other on-logon stuff */
void ConnectUser(userrec *user)
{
	// dns is already done, things are fast. no need to wait for dns to complete just pass them straight on
	if ((user->dns_done) && (user->registered >= 3) && (AllModulesReportReady(user)))
	{
		FullConnectUser(user);
	}
}

std::string GetVersionString()
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

void handle_version(char **parameters, int pcnt, userrec *user)
{
	WriteServ(user->fd,"351 %s :%s",user->nick,GetVersionString().c_str());
}


bool is_valid_cmd(const char* commandname, int pcnt, userrec * user)
{
	for (unsigned int i = 0; i < cmdlist.size(); i++)
	{
		if (!strcasecmp(cmdlist[i].command,commandname))
		{
			if (cmdlist[i].handler_function)
			{
				if ((pcnt>=cmdlist[i].min_params) && (strcasecmp(cmdlist[i].source,"<core>")))
				{
					if ((strchr(user->modes,cmdlist[i].flags_needed)) || (!cmdlist[i].flags_needed))
					{
						if (cmdlist[i].flags_needed)
						{
							if ((user->HasPermission((char*)commandname)) || (is_uline(user->server)))
							{
								return true;
							}
							else
							{
								return false;
							}
						}
						return true;
					}
				}
			}
		}
	}
	return false;
}

// calls a handler function for a command

void call_handler(const char* commandname,char **parameters, int pcnt, userrec *user)
{
	for (unsigned int i = 0; i < cmdlist.size(); i++)
	{
		if (!strcasecmp(cmdlist[i].command,commandname))
		{
			if (cmdlist[i].handler_function)
			{
				if (pcnt>=cmdlist[i].min_params)
				{
					if ((strchr(user->modes,cmdlist[i].flags_needed)) || (!cmdlist[i].flags_needed))
					{
						if (cmdlist[i].flags_needed)
						{
							if ((user->HasPermission((char*)commandname)) || (is_uline(user->server)))
							{
								cmdlist[i].handler_function(parameters,pcnt,user);
							}
						}
						else
						{
							cmdlist[i].handler_function(parameters,pcnt,user);
						}
					}
				}
			}
		}
	}
}


void force_nickchange(userrec* user,const char* newnick)
{
	char nick[MAXBUF];
	int MOD_RESULT = 0;
	
	strcpy(nick,"");

	FOREACH_RESULT(OnUserPreNick(user,newnick));
	if (MOD_RESULT) {
		stats->statsCollisions++;
		kill_link(user,"Nickname collision");
		return;
	}
	if (matches_qline(newnick))
	{
		stats->statsCollisions++;
		kill_link(user,"Nickname collision");
		return;
	}
	
	if (user)
	{
		if (newnick)
		{
			strncpy(nick,newnick,MAXBUF);
		}
		if (user->registered == 7)
		{
			char* pars[1];
			pars[0] = nick;
			handle_nick(pars,1,user);
		}
	}
}
				

int process_parameters(char **command_p,char *parameters)
{
	int j = 0;
	int q = strlen(parameters);
	if (!q)
	{
		/* no parameters, command_p invalid! */
		return 0;
	}
	if (parameters[0] == ':')
	{
		command_p[0] = parameters+1;
		return 1;
	}
	if (q)
	{
		if ((strchr(parameters,' ')==NULL) || (parameters[0] == ':'))
		{
			/* only one parameter */
			command_p[0] = parameters;
			if (parameters[0] == ':')
			{
				if (strchr(parameters,' ') != NULL)
				{
					command_p[0]++;
				}
			}
			return 1;
		}
	}
	command_p[j++] = parameters;
	for (int i = 0; i <= q; i++)
	{
		if (parameters[i] == ' ')
		{
			command_p[j++] = parameters+i+1;
			parameters[i] = '\0';
			if (command_p[j-1][0] == ':')
			{
				*command_p[j-1]++; /* remove dodgy ":" */
				break;
				/* parameter like this marks end of the sequence */
			}
		}
	}
	return j; /* returns total number of items in the list */
}

void process_command(userrec *user, char* cmd)
{
	char *parameters;
	char *command;
	char *command_p[127];
	char p[MAXBUF], temp[MAXBUF];
	int j, items, cmd_found;

	for (int i = 0; i < 127; i++)
		command_p[i] = NULL;

	if (!user)
	{
		return;
	}
	if (!cmd)
	{
		return;
	}
	if (!cmd[0])
	{
		return;
	}
	
	int total_params = 0;
	if (strlen(cmd)>2)
	{
		for (unsigned int q = 0; q < strlen(cmd)-1; q++)
		{
			if ((cmd[q] == ' ') && (cmd[q+1] == ':'))
			{
				total_params++;
				// found a 'trailing', we dont count them after this.
				break;
			}
			if (cmd[q] == ' ')
				total_params++;
		}
	}

	// another phidjit bug...
	if (total_params > 126)
	{
		*(strchr(cmd,' ')) = '\0';
		WriteServ(user->fd,"421 %s %s :Too many parameters given",user->nick,cmd);
		return;
	}

	strlcpy(temp,cmd,MAXBUF);
	
	std::string tmp = cmd;
	for (int i = 0; i <= MODCOUNT; i++)
	{
		std::string oldtmp = tmp;
		modules[i]->OnServerRaw(tmp,true,user);
		if (oldtmp != tmp)
		{
			log(DEBUG,"A Module changed the input string!");
			log(DEBUG,"New string: %s",tmp.c_str());
			log(DEBUG,"Old string: %s",oldtmp.c_str());
			break;
		}
	}
  	strlcpy(cmd,tmp.c_str(),MAXBUF);
	strlcpy(temp,cmd,MAXBUF);

	if (!strchr(cmd,' '))
	{
		/* no parameters, lets skip the formalities and not chop up
		 * the string */
		log(DEBUG,"About to preprocess command with no params");
		items = 0;
		command_p[0] = NULL;
		parameters = NULL;
		for (unsigned int i = 0; i <= strlen(cmd); i++)
		{
			cmd[i] = toupper(cmd[i]);
		}
		command = cmd;
	}
	else
	{
		strcpy(cmd,"");
		j = 0;
		/* strip out extraneous linefeeds through mirc's crappy pasting (thanks Craig) */
		for (unsigned int i = 0; i < strlen(temp); i++)
		{
			if ((temp[i] != 10) && (temp[i] != 13) && (temp[i] != 0) && (temp[i] != 7))
			{
				cmd[j++] = temp[i];
				cmd[j] = 0;
			}
		}
		/* split the full string into a command plus parameters */
		parameters = p;
		strcpy(p," ");
		command = cmd;
		if (strchr(cmd,' '))
		{
			for (unsigned int i = 0; i <= strlen(cmd); i++)
			{
				/* capitalise the command ONLY, leave params intact */
				cmd[i] = toupper(cmd[i]);
				/* are we nearly there yet?! :P */
				if (cmd[i] == ' ')
				{
					command = cmd;
					parameters = cmd+i+1;
					cmd[i] = '\0';
					break;
				}
			}
		}
		else
		{
			for (unsigned int i = 0; i <= strlen(cmd); i++)
			{
				cmd[i] = toupper(cmd[i]);
			}
		}

	}
	cmd_found = 0;
	
	if (strlen(command)>MAXCOMMAND)
	{
		WriteServ(user->fd,"421 %s %s :Command too long",user->nick,command);
		return;
	}
	
	for (unsigned int x = 0; x < strlen(command); x++)
	{
		if (((command[x] < 'A') || (command[x] > 'Z')) && (command[x] != '.'))
		{
			if (((command[x] < '0') || (command[x]> '9')) && (command[x] != '-'))
			{
				if (strchr("@!\"$%^&*(){}[]_=+;:'#~,<>/?\\|`",command[x]))
				{
					stats->statsUnknown++;
					WriteServ(user->fd,"421 %s %s :Unknown command",user->nick,command);
					return;
				}
			}
		}
	}

	for (unsigned int i = 0; i != cmdlist.size(); i++)
	{
		if (cmdlist[i].command[0])
		{
			if (strlen(command)>=(strlen(cmdlist[i].command))) if (!strncmp(command, cmdlist[i].command,MAXCOMMAND))
			{
				if (parameters)
				{
					if (parameters[0])
					{
						items = process_parameters(command_p,parameters);
					}
					else
					{
						items = 0;
						command_p[0] = NULL;
					}
				}
				else
				{
					items = 0;
					command_p[0] = NULL;
				}
				
				if (user)
				{
					/* activity resets the ping pending timer */
					user->nping = TIME + user->pingmax;
					if ((items) < cmdlist[i].min_params)
					{
					        log(DEBUG,"process_command: not enough parameters: %s %s",user->nick,command);
						WriteServ(user->fd,"461 %s %s :Not enough parameters",user->nick,command);
						return;
					}
					if ((!strchr(user->modes,cmdlist[i].flags_needed)) && (cmdlist[i].flags_needed))
					{
					        log(DEBUG,"process_command: permission denied: %s %s",user->nick,command);
						WriteServ(user->fd,"481 %s :Permission Denied- You do not have the required operator privilages",user->nick);
						cmd_found = 1;
						return;
					}
					if ((cmdlist[i].flags_needed) && (!user->HasPermission(command)))
					{
					        log(DEBUG,"process_command: permission denied: %s %s",user->nick,command);
						WriteServ(user->fd,"481 %s :Permission Denied- Oper type %s does not have access to command %s",user->nick,user->oper,command);
						cmd_found = 1;
						return;
					}
					/* if the command isnt USER, PASS, or NICK, and nick is empty,
					 * deny command! */
					if ((strncmp(command,"USER",4)) && (strncmp(command,"NICK",4)) && (strncmp(command,"PASS",4)))
					{
						if ((!isnick(user->nick)) || (user->registered != 7))
						{
						        log(DEBUG,"process_command: not registered: %s %s",user->nick,command);
							WriteServ(user->fd,"451 %s :You have not registered",command);
							return;
						}
					}
					if ((user->registered == 7) && (!strchr(user->modes,'o')))
					{
						std::stringstream dcmds(Config->DisabledCommands);
						while (!dcmds.eof())
						{
							std::string thiscmd;
							dcmds >> thiscmd;
							if (!strcasecmp(thiscmd.c_str(),command))
							{
								// command is disabled!
								WriteServ(user->fd,"421 %s %s :This command has been disabled.",user->nick,command);
								return;
							}
						}
					}
					if ((user->registered == 7) || (!strncmp(command,"USER",4)) || (!strncmp(command,"NICK",4)) || (!strncmp(command,"PASS",4)))
					{
						if (cmdlist[i].handler_function)
						{
							
							/* ikky /stats counters */
							if (temp)
							{
								cmdlist[i].use_count++;
								cmdlist[i].total_bytes+=strlen(temp);
							}

							int MOD_RESULT = 0;
							FOREACH_RESULT(OnPreCommand(command,command_p,items,user));
							if (MOD_RESULT == 1) {
								return;
							}

							/* WARNING: nothing may come after the
							 * command handler call, as the handler
							 * may free the user structure! */

							cmdlist[i].handler_function(command_p,items,user);
						}
						return;
					}
					else
					{
						WriteServ(user->fd,"451 %s :You have not registered",command);
						return;
					}
				}
				cmd_found = 1;
			}
		}
	}
	if ((!cmd_found) && (user))
	{
		stats->statsUnknown++;
		WriteServ(user->fd,"421 %s %s :Unknown command",user->nick,command);
	}
}

bool removecommands(const char* source)
{
	bool go_again = true;
	while (go_again)
	{
		go_again = false;
		for (std::deque<command_t>::iterator i = cmdlist.begin(); i != cmdlist.end(); i++)
		{
			if (!strcmp(i->source,source))
			{
				log(DEBUG,"removecommands(%s) Removing dependent command: %s",i->source,i->command);
				cmdlist.erase(i);
				go_again = true;
				break;
			}
		}
	}
	return true;
}


void process_buffer(const char* cmdbuf,userrec *user)
{
	if (!user)
	{
		log(DEFAULT,"*** BUG *** process_buffer was given an invalid parameter");
		return;
	}
	char cmd[MAXBUF];
	if (!cmdbuf)
	{
		log(DEFAULT,"*** BUG *** process_buffer was given an invalid parameter");
		return;
	}
	if (!cmdbuf[0])
	{
		return;
	}
	while (*cmdbuf == ' ') cmdbuf++; // strip leading spaces

	strlcpy(cmd,cmdbuf,MAXBUF);
	if (!cmd[0])
	{
		return;
	}
	int sl = strlen(cmd)-1;
	if ((cmd[sl] == 13) || (cmd[sl] == 10))
	{
		cmd[sl] = '\0';
	}
	sl = strlen(cmd)-1;
	if ((cmd[sl] == 13) || (cmd[sl] == 10))
	{
		cmd[sl] = '\0';
	}
	sl = strlen(cmd)-1;
	while (cmd[sl] == ' ') // strip trailing spaces
	{
		cmd[sl] = '\0';
		sl = strlen(cmd)-1;
	}

	if (!cmd[0])
	{
		return;
	}
        log(DEBUG,"CMDIN: %s %s",user->nick,cmd);
	tidystring(cmd);
	if ((user) && (cmd))
	{
		process_command(user,cmd);
	}
}

char MODERR[MAXBUF];

char* ModuleError()
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
                        removecommands(filename);
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
	for (int count = 0; count < BoundPortCount; count++)
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
						for (std::vector<InspSocket*>::iterator a = this->module_sockets.begin(); a < this->module_sockets.end(); a++)
						{
							s_del = (InspSocket*)*a;
							if ((s_del) && (s_del->GetFd() == activefds[activefd]))
							{
								this->module_sockets.erase(a);
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
							FOREACH_MOD OnRawSocketAccept(incomingSockfd, target, in_port);
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

