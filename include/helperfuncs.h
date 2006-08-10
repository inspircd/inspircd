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

/** Debug levels for use with InspIRCd::Log()
 */
enum DebugLevel
{
	DEBUG = 10,
	VERBOSE = 20,
	DEFAULT = 30,
	SPARSE = 40,
	NONE = 50,
};

/* I'm not entirely happy with this, the ## before 'args' is a g++ extension.
 * The problem is that if you #define log(l, x, args...) and then call it
 * with only two parameters, you get do_log(l, x, ), which is a syntax error...
 * The ## tells g++ to remove the trailing comma...
 * If this is ever an issue, we can just have an #ifndef GCC then #define log(a...) do_log(a)
 */
#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x) 
#define log(l, x, args...) InspIRCd::Log(l, __FILE__ ":" STRINGIFY(__LINE__) ": " x, ##args)

void strlower(char *n);
void Error(int status);
void ShowMOTD(userrec *user);
void ShowRULES(userrec *user);
bool AllModulesReportReady(userrec* user);
int InsertMode(std::string &output, const char* modes, unsigned short section);
bool IsValidChannelName(const char *);

#endif
