/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
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

#ifndef __INSPIRCD_H__
#define __INSPIRCD_H__

#include "inspircd_config.h"
#include <string>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>

#ifndef _LINUX_C_LIB_VERSION
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#endif

#include <arpa/inet.h>
#include <string>
#include <deque>

#include "inspircd_io.h"
#include "inspircd_util.h"
#include "users.h"
#include "channels.h"
#include "socket.h"

// some misc defines

#define ERROR -1
#define TRUE 1
#define FALSE 0
#define MAXSOCKS 64
#define MAXCOMMAND 32

// flags for use with WriteMode

#define WM_AND 1
#define WM_OR 2

// flags for use with OnUserPreMessage and OnUserPreNotice

#define TYPE_USER 1
#define TYPE_CHANNEL 2
#define TYPE_SERVER 3

#define IS_LOCAL(x) (x->fd > -1)
#define IS_REMOTE(x) (x->fd < 0)
#define IS_MODULE_CREATED(x) (x->fd == FD_MAGIC_NUMBER)

typedef void (handlerfunc) (char**, int, userrec*);

class serverstats
{
  public:
	int statsAccept;
	int statsRefused;
	int statsUnknown;
	int statsCollisions;
	int statsDns;
	int statsDnsGood;
	int statsDnsBad;
	int statsConnects;
	int statsSent;
	int statsRecv;

	serverstats()
	{
		statsAccept = statsRefused = statsUnknown = 0;
		statsCollisions = statsDns = statsDnsGood = 0;
		statsDnsBad = statsConnects = statsSent = statsRecv = 0;
	}
};


class InspIRCd
{

 private:
	char MODERR[MAXBUF];
        void erase_factory(int j);
        void erase_module(int j);	

 public:
	time_t startup_time;

	std::string GetRevision();
	std::string GetVersionString();
	char* ModuleError();
	bool LoadModule(const char* filename);
	bool UnloadModule(const char* filename);
	InspIRCd(int argc, char** argv);
	int Run();

};

/* prototypes */
void force_nickchange(userrec* user,const char* newnick);
void call_handler(std::string &commandname,char **parameters, int pcnt, userrec *user);
bool is_valid_cmd(std::string &commandname, int pcnt, userrec * user);
int loop_call(handlerfunc fn, char **parameters, int pcnt, userrec *u, int start, int end, int joins);
userrec* ReHashNick(char* Old, char* New);
/* userrec optimization stuff */
void AddServerName(std::string servername);
const char* FindServerNamePtr(std::string servername);
void* dns_task(void* arg);
void process_buffer(const char* cmdbuf,userrec *user);

#endif
