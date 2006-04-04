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

void log(int level,char *text, ...);
void readfile(file_cache &F, const char* fname);

void Write(int sock,char *text, ...);
void WriteServ(int sock, char* text, ...);
void WriteFrom(int sock, userrec *user,char* text, ...);
void WriteTo(userrec *source, userrec *dest,char *data, ...);
void WriteChannel(chanrec* Ptr, userrec* user, char* text, ...);
void WriteChannelLocal(chanrec* Ptr, userrec* user, char* text, ...);
void WriteChannelWithServ(char* ServName, chanrec* Ptr, char* text, ...);
void ChanExceptSender(chanrec* Ptr, userrec* user, char status, char* text, ...);

void Write_NoFormat(int sock,const char *text);
void WriteServ_NoFormat(int sock, const char* text);
void WriteFrom_NoFormat(int sock, userrec *user,const char* text);
void WriteTo_NoFormat(userrec *source, userrec *dest,const char *data);
void WriteChannel_NoFormat(chanrec* Ptr, userrec* user, const char* text);
void WriteChannelLocal_NoFormat(chanrec* Ptr, userrec* user, const char* text);
void WriteChannelWithServ_NoFormat(char* ServName, chanrec* Ptr, const char* text);
void ChanExceptSender_NoFormat(chanrec* Ptr, userrec* user, char status, const char* text);
void WriteCommon_NoFormat(userrec *u, const char* text);
void WriteCommonExcept_NoFormat(userrec *u, const char* text);

std::string GetServerDescription(char* servername);
void WriteCommon(userrec *u, char* text, ...);
void WriteCommonExcept(userrec *u, char* text, ...);
void WriteOpers(const char* text, ...);
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
std::string GetFullProgDir(char** argv, int argc);
int InsertMode(std::string &output, const char* modes, unsigned short section);
bool IsValidChannelName(const char *);

int charlcat(char* x,char y,int z);
bool charremove(char* mp, char remove);

#endif
