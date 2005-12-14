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

#ifndef __INSPIRCD_IO_H__
#define __INSPIRCD_IO_H__

#include <sstream>
#include <string>
#include "inspircd.h"

// flags for use with log()

#define DEBUG 10
#define VERBOSE 20
#define DEFAULT 30
#define SPARSE 40
#define NONE 50

class ServerConfig
{
  public:
	char ServerName[MAXBUF];
	char Network[MAXBUF];
	char ServerDesc[MAXBUF];
	char AdminName[MAXBUF];
	char AdminEmail[MAXBUF];
	char AdminNick[MAXBUF];
	char diepass[MAXBUF];
	char restartpass[MAXBUF];
	char motd[MAXBUF];
	char rules[MAXBUF];
	char PrefixQuit[MAXBUF];
	char DieValue[MAXBUF];
	char DNSServer[MAXBUF];
	char DisabledCommands[MAXBUF];
        char ModPath[1024];
        char MyExecutable[1024];
        FILE *log_file;
        bool nofork;
        bool unlimitcore;
        bool AllowHalfop;
        int dns_timeout;
        int NetBufferSize;      // NetBufferSize used as the buffer size for all read() ops
        int MaxConn;            // size of accept() backlog (128 by default on *BSD)
        unsigned int SoftLimit;
        int MaxWhoResults;
        int debugging;
        int LogLevel;
        int DieDelay;
        char addrs[MAXBUF][255];
	file_cache MOTD;
	file_cache RULES;
	char PID[1024];
	std::stringstream config_f;

	ServerConfig()
	{
		*ServerName = '\0';
		*Network = '\0';
		*ServerDesc = '\0';
		*AdminName = '\0';
		*AdminEmail = '\0';
		*AdminNick = '\0';
		*diepass = '\0';
		*restartpass = '\0';
		*motd = '\0';
		*rules = '\0';
		*PrefixQuit = '\0';
		*DieValue = '\0';
		*DNSServer = '\0';
		*ModPath = '\0';
		*MyExecutable = '\0';
		*DisabledCommands = '\0';
		*PID = '\0';
		log_file = NULL;
		nofork = false;
		unlimitcore = false;
		AllowHalfop = true;
		dns_timeout = 5;
		NetBufferSize = 10240;
		SoftLimit = MAXCLIENTS;
		MaxConn = SOMAXCONN;
		MaxWhoResults = 100;
		debugging = 0;
		LogLevel = DEFAULT;
		DieDelay = 5;
	}
};


void Exit (int); 
void Start (void); 
int DaemonSeed (void); 
bool FileExists (const char* file);
int OpenTCPSocket (void); 
int BindSocket (int sockfd, struct sockaddr_in client, struct sockaddr_in server, int port, char* addr);

bool LoadConf(const char* filename, std::stringstream *target, std::stringstream* errorstream);
int ConfValue(char* tag, char* var, int index, char *result, std::stringstream *config);
int ReadConf(std::stringstream *config_f,const char* tag, const char* var, int index, char *result);
int ConfValueEnum(char* tag,std::stringstream *config);
int EnumConf(std::stringstream *config_f,const char* tag);
int EnumValues(std::stringstream *config, const char* tag, int index);
void WritePID(std::string filename);

#endif
