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
#include <vector>
#include "inspircd.h"
#include "globals.h"

// flags for use with log()

#define DEBUG 10
#define VERBOSE 20
#define DEFAULT 30
#define SPARSE 40
#define NONE 50

class ServerConfig : public classbase
{
  private:
	std::vector<std::string> include_stack;
	int fgets_safe(char* buffer, size_t maxsize, FILE* &file);
	std::string ConfProcess(char* buffer, long linenumber, std::stringstream* errorstream, bool &error, std::string filename);

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
	ClassVector Classes;
	std::vector<std::string> module_names;

	ServerConfig();
	void ClearStack();
	void Read(bool bail, userrec* user);
	bool LoadConf(const char* filename, std::stringstream *target, std::stringstream* errorstream);
	int ConfValue(char* tag, char* var, int index, char *result, std::stringstream *config);
	int ReadConf(std::stringstream *config_f,const char* tag, const char* var, int index, char *result);
	int ConfValueEnum(char* tag,std::stringstream *config);
	int EnumConf(std::stringstream *config_f,const char* tag);
	int EnumValues(std::stringstream *config, const char* tag, int index);
};


void Exit (int); 
void Start (void); 
int DaemonSeed (void); 
bool FileExists (const char* file);
int OpenTCPSocket (void); 
int BindSocket (int sockfd, struct sockaddr_in client, struct sockaddr_in server, int port, char* addr);

/*bool LoadConf(const char* filename, std::stringstream *target, std::stringstream* errorstream);
int ConfValue(char* tag, char* var, int index, char *result, std::stringstream *config);
int ReadConf(std::stringstream *config_f,const char* tag, const char* var, int index, char *result);
int ConfValueEnum(char* tag,std::stringstream *config);
int EnumConf(std::stringstream *config_f,const char* tag);
int EnumValues(std::stringstream *config, const char* tag, int index);*/

void WritePID(std::string filename);

#endif
