/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef _HELPER_H_
#define _HELPER_H_

#include "dynamic.h"
#include "base.h"
#include "ctables.h"
#include "users.h"
#include "channels.h"
#include "typedefs.h"
#include <string>
#include <deque>
#include <sstream>

/** Flags for use with log()
 */
#define DEBUG 10
#define VERBOSE 20
#define DEFAULT 30
#define SPARSE 40
#define NONE 50

/* I'm not entirely happy with this, the ## before 'args' is a g++ extension.
 * The problem is that if you #define log(l, x, args...) and then call it
 * with only two parameters, you get do_log(l, x, ), which is a syntax error...
 * The ## tells g++ to remove the trailing comma...
 * If this is ever an issue, we can just have an #ifndef GCC then #define log(a...) do_log(a)
 */
#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x) 
#define log(l, x, args...) do_log(l, __FILE__ ":" STRINGIFY(__LINE__) ": " x, ##args)

void do_log(int level, const char *text, ...);
void readfile(file_cache &F, const char* fname);

void Write(int sock,char *text, ...);
void WriteServ(int sock, char* text, ...);
void WriteFrom(int sock, userrec *user,char* text, ...);
void WriteTo(userrec *source, userrec *dest,char *data, ...);
void WriteCommon(userrec *u, char* text, ...);
void WriteCommonExcept(userrec *u, char* text, ...);
void WriteOpers(const char* text, ...);

void Write_NoFormat(int sock,const char *text);
void WriteServ_NoFormat(int sock, const char* text);
void WriteFrom_NoFormat(int sock, userrec *user,const char* text);
void WriteTo_NoFormat(userrec *source, userrec *dest,const char *data);
void WriteCommon_NoFormat(userrec *u, const char* text);
void WriteCommonExcept_NoFormat(userrec *u, const char* text);
void WriteOpers_NoFormat(const char* text);

std::string GetServerDescription(const char* servername);
void WriteMode(const char* modes, int flags, const char* text, ...);
void NoticeAll(userrec *source, bool local_only, char* text, ...);
void ServerNoticeAll(char* text, ...);
void ServerPrivmsgAll(char* text, ...);
void WriteWallOps(userrec *source, bool local_only, char* text, ...);
void strlower(char *n);
userrec* Find(const std::string &nick);
userrec* Find(const char* nick);
chanrec* FindChan(const char* chan);
long GetMaxBans(char* name);
void purge_empty_chans(userrec* u);
char* chanmodes(chanrec *chan, bool showkey);
void userlist(userrec *user,chanrec *c);
int usercount_i(chanrec *c);
int usercount(chanrec *c);
ConnectClass GetClass(userrec *user);
void send_error(char *s);
void Error(int status);
int usercnt(void);
int registered_usercount(void);
int usercount_invisible(void);
int usercount_opers(void);
int usercount_unknown(void);
long chancount(void);
long local_count();
void ShowMOTD(userrec *user);
void ShowRULES(userrec *user);
bool AllModulesReportReady(userrec* user);
bool DirValid(char* dirandfile);
bool FileExists(const char* file);
char* CleanFilename(char* name);
std::string GetFullProgDir(char** argv, int argc);
int InsertMode(std::string &output, const char* modes, unsigned short section);
bool IsValidChannelName(const char *);

int charlcat(char* x,char y,int z);
bool charremove(char* mp, char remove);

#endif
