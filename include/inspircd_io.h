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
 
 $Log$
 Revision 1.1  2003/01/23 19:45:58  brain
 Initial revision

 Revision 1.5  2003/01/21 20:31:24  brain
 Modified to add documentation
 Added ConfigReader class for modules

 Revision 1.4  2003/01/06 23:38:29  brain

 just playing with header tags


 * ---------------------------------------------------
 */

void Exit (int); 
void Start (void); 
int DaemonSeed (void); 
int CheckConfig (void); 
int OpenTCPSocket (void); 
int BindSocket (int sockfd, struct sockaddr_in client, struct sockaddr_in server, int port, char* addr);

int ConfValue(char* tag, char* var, int index, char *result);
int ReadConf(const char* filename,const char* tag, const char* var, int index, char *result);
int ConfValueEnum(char* tag);
int EnumConf(const char* filename,const char* tag);

