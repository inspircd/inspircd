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

void Exit (int); 
void Start (void); 
int DaemonSeed (void); 
int CheckModule (char module[MAXBUF]);
int CheckConfig (void); 
int OpenTCPSocket (void); 
int BindSocket (int sockfd, struct sockaddr_in client, struct sockaddr_in server, int port, char* addr);

int ConfValue(char* tag, char* var, int index, char *result);
int ReadConf(const char* filename,const char* tag, const char* var, int index, char *result);
int ConfValueEnum(char* tag);
int EnumConf(const char* filename,const char* tag);

