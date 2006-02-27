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
#include "users.h"
#include "channels.h"
#include "socket.h"
#include "mode.h"
#include "socketengine.h"
#include "command_parse.h"

// some misc defines

#define ERROR -1
#define TRUE 1
#define FALSE 0
#define MAXSOCKS 64
#define MAXCOMMAND 32

#define ETIREDGERBILS EAGAIN

/*
flags for use with WriteMode

#define WM_AND 1
#define WM_OR 2

flags for use with OnUserPreMessage and OnUserPreNotice

#define TYPE_USER 1
#define TYPE_CHANNEL 2
#define TYPE_SERVER 3

#define IS_LOCAL(x) (x->fd > -1)
#define IS_REMOTE(x) (x->fd < 0)
#define IS_MODULE_CREATED(x) (x->fd == FD_MAGIC_NUMBER)
*/

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
	int BoundPortCount;

	serverstats()
	{
		statsAccept = statsRefused = statsUnknown = 0;
		statsCollisions = statsDns = statsDnsGood = 0;
		statsDnsBad = statsConnects = statsSent = statsRecv = 0;
		BoundPortCount = 0;
	}
};


class InspIRCd
{

 private:
	char MODERR[MAXBUF];
        void erase_factory(int j);
        void erase_module(int j);
	void BuildISupport();
	void MoveTo(std::string modulename,int slot);
        bool expire_run;
        int activefds[MAX_DESCRIPTORS];
        int incomingSockfd;
        int in_port;
        userrec* cu;
        InspSocket* s;
        InspSocket* s_del;
        unsigned int numberactive;
        sockaddr_in sock_us;     // our port number
        socklen_t uslen;         // length of our port number

 public:
	time_t startup_time;
	ModeParser* ModeGrok;
	CommandParser* Parser;
	SocketEngine* SE;
	serverstats* stats;

	void MakeLowerMap();
	std::string GetRevision();
	std::string GetVersionString();
	char* ModuleError();
	bool LoadModule(const char* filename);
	bool UnloadModule(const char* filename);
	void MoveToLast(std::string modulename);
	void MoveToFirst(std::string modulename);
	void MoveAfter(std::string modulename, std::string after);
	void MoveBefore(std::string modulename, std::string before);
	InspIRCd(int argc, char** argv);
	void DoOneIteration(bool process_module_sockets);
	int Run();

};

/* userrec optimization stuff */
void AddServerName(std::string servername);
const char* FindServerNamePtr(std::string servername);
bool FindServerName(std::string servername);

#endif
