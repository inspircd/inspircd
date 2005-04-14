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

#include <string>
#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <netdb.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/param.h>
#include <sys/types.h>

#ifndef _LINUX_C_LIB_VERSION
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#endif

#include <arpa/inet.h>
#include <string>
#include <deque>

#include "inspircd_config.h"
#include "inspircd_io.h"
#include "inspircd_util.h"
#include "users.h"
#include "channels.h"
#include "servers.h"

// some misc defines

#define ERROR -1
#define TRUE 1
#define FALSE 0
#define IDENTMAX 12
#define MAXSOCKS 64

// maximum lengths of items

#define MAXQUIT 255
#define MAXCOMMAND 32
#define MAXTOPIC 307
#define MAXKICK 255

// flags for use with log()

#define DEBUG 10
#define VERBOSE 20
#define DEFAULT 30
#define SPARSE 40
#define NONE 50

// flags for use with WriteMode

#define WM_AND 1
#define WM_OR 2

// flags for use with OnUserPreMessage and OnUserPreNotice

#define TYPE_USER 1
#define TYPE_CHANNEL 2

typedef std::deque<std::string> file_cache;

typedef void (handlerfunc) (char**, int, userrec*);

/* prototypes */
int InspIRCd(void);
int InitConfig(void);
void Error(int status);
void send_error(char *s);
void ReadConfig(bool bail,userrec* user);
void strlower(char *n);

void WriteOpers(char* text, ...);
void WriteMode(const char* modes, int flags, const char* text, ...);
void log(int level, char *text, ...);
void Write(int sock,char *text, ...);
void WriteServ(int sock, char* text, ...);
void WriteFrom(int sock, userrec *user,char* text, ...);
void WriteTo(userrec *source, userrec *dest,char *data, ...);
void WriteChannel(chanrec* Ptr, userrec* user, char* text, ...);
void ChanExceptSender(chanrec* Ptr, userrec* user, char* text, ...);
void WriteCommon(userrec *u, char* text, ...);
void WriteCommonExcept(userrec *u, char* text, ...);
void WriteWallOps(userrec *source, bool local_only, char* text, ...);
void WriteChannelLocal(chanrec* Ptr, userrec* user, char* text, ...);
void WriteChannelWithServ(char* ServerName, chanrec* Ptr, userrec* user, char* text, ...);
char* chanmodes(chanrec *chan);
userrec* Find(std::string nick);
chanrec* FindChan(const char* chan);
std::string getservername();
std::string getserverdesc();
std::string getnetworkname();
std::string getadminname();
std::string getadminemail();
std::string getadminnick();
void readfile(file_cache &F, const char* fname);
bool ModeDefined(char c, int i);
bool ModeDefinedOper(char c, int i);
int ModeDefinedOn(char c, int i);
int ModeDefinedOff(char c, int i);
void ModeMakeList(char modechar);
bool ModeIsListMode(char modechar, int type);
chanrec* add_channel(userrec *user, const char* cn, const char* key, bool override);
chanrec* del_channel(userrec *user, const char* cname, const char* reason, bool local);
void force_nickchange(userrec* user,const char* newnick);
void kill_link(userrec *user,const char* r);
void kill_link_silent(userrec *user,const char* r);
int usercount(chanrec *c);
void call_handler(const char* commandname,char **parameters, int pcnt, userrec *user);
long GetRevision();
int loop_call(handlerfunc fn, char **parameters, int pcnt, userrec *u, int start, int end, int joins);
void kick_channel(userrec *src,userrec *user, chanrec *Ptr, char* reason);
void purge_empty_chans(void);
char* Passwd(userrec *user);
bool IsDenied(userrec *user);
void AddWhoWas(userrec* u);
void userlist(userrec *user,chanrec *c);
std::string GetServerDescription(char* servername);
int usercnt(void);
int registered_usercount(void);
int usercount_invisible(void);
int usercount_opers(void);
int usercount_unknown(void);
long chancount(void);
long count_servs(void);
long servercount(void);
long local_count();
void ShowMOTD(userrec *user);
void ShowRULES(userrec *user);
int usercount(chanrec *c);
int usercount_i(chanrec *c);
void update_stats_l(int fd,int data_out);
void ConnectUser(userrec *user);
void DoSplitEveryone();
char islast(const char* s);
long map_count(const char* s);
userrec* ReHashNick(char* Old, char* New);
long GetMaxBans(char* name);
bool LoadModule(const char* filename);
bool UnloadModule(const char* filename);
char* ModuleError();

// mesh network functions

void NetSendToCommon(userrec* u, char* s);
void NetSendToAll(char* s);
void NetSendToAllAlive(char* s);
void NetSendToOne(char* target,char* s);
void NetSendToAllExcept(const char* target,char* s);
void NetSendMyRoutingTable();
void DoSplit(const char* params);
void RemoveServer(const char* name);
void DoSync(serverrec* serv, char* tcp_host);


