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

#ifndef __MESSAGE_H
#define __MESSAGE_H

// include the common header files

#include <typeinfo>
#include <iostream>
#include <string>
#include <deque>
#include <sstream>
#include <vector>
#include "users.h"
#include "channels.h"

int common_channels(userrec *u, userrec *u2);
void chop(char* str);
void tidystring(char* str);
void safedelete(chanrec *p);
void safedelete(userrec *p);
void Blocking(int s);
void NonBlocking(int s);
int CleanAndResolve (char *resolvedHost, const char *unresolvedHost);
int c_count(userrec* u);
bool hasumode(userrec* user, char mode);
void ChangeName(userrec* user, const char* gecos);
void ChangeDisplayedHost(userrec* user, const char* host);
int isident(const char* n);
int isnick(const char* n);
char* cmode(userrec *user, chanrec *chan);
int cstatus(userrec *user, chanrec *chan);
int has_channel(userrec *u, chanrec *c);
void TidyBan(char *ban);
char* chlist(userrec *user);
void send_network_quit(const char* nick, const char* reason);

#endif
