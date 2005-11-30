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

// some misc defines

#define ERROR -1
#define TRUE 1
#define FALSE 0
#define MAXSOCKS 64
#define MAXCOMMAND 32

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
#define TYPE_SERVER 3

typedef std::deque<std::string> file_cache;

typedef void (handlerfunc) (char**, int, userrec*);

/* prototypes */
int InspIRCd(char** argv, int argc);
int InitConfig(void);
void ReadConfig(bool bail,userrec* user);

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
void call_handler(const char* commandname,char **parameters, int pcnt, userrec *user);
std::string GetRevision();
int loop_call(handlerfunc fn, char **parameters, int pcnt, userrec *u, int start, int end, int joins);
void kick_channel(userrec *src,userrec *user, chanrec *Ptr, char* reason);
void AddWhoWas(userrec* u);
void update_stats_l(int fd,int data_out);
void ConnectUser(userrec *user);
void DoSplitEveryone();
userrec* ReHashNick(char* Old, char* New);
bool LoadModule(const char* filename);
bool UnloadModule(const char* filename);
char* ModuleError();
void NoticeAll(userrec *source, bool local_only, char* text, ...);
void NoticeAllOpers(userrec *source, bool local_only, char* text, ...);

// optimization tricks to save us walking the user hash

void AddOper(userrec* user);
void DeleteOper(userrec* user);

void handle_version(char **parameters, int pcnt, userrec *user);

// userrec optimization stuff

void AddServerName(std::string servername);
const char* FindServerNamePtr(std::string servername);

std::string GetVersionString();

