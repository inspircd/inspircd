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

#include <sstream>
#include <string>

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
