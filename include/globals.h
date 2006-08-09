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

#ifndef __WORLD_H
#define __WORLD_H

// include the common header files

#include <typeinfo>
#include <iostream>
#include <string>
#include <deque>
#include "users.h"
#include "channels.h"

typedef std::deque<std::string> file_cache;
typedef std::pair< std::string, std::string > KeyVal;
typedef std::vector< KeyVal > KeyValList;
typedef std::multimap< std::string, KeyValList > ConfigDataHash;

void WriteOpers(char* text, ...);
void do_log(int level, char *text, ...);
int isnick(const char *n);
chanrec* FindChan(const char* chan);
void readfile(file_cache &F, const char* fname);

#endif
