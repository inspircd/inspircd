/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2003 ChatSpike-Dev.
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

void Exit (int); 
void Start (void); 
int DaemonSeed (void); 
int FileExists (char* file);
int OpenTCPSocket (void); 
int BindSocket (int sockfd, struct sockaddr_in client, struct sockaddr_in server, int port, char* addr);

void LoadConf(const char* filename, std::stringstream *target);
int ConfValue(char* tag, char* var, int index, char *result, std::stringstream *config);
int ReadConf(std::stringstream *config_f,const char* tag, const char* var, int index, char *result);
int ConfValueEnum(char* tag,std::stringstream *config);
int EnumConf(std::stringstream *config_f,const char* tag);

