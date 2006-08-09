/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
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

int isident(const char* n);
int isnick(const char* n);
const char* cmode(userrec *user, chanrec *chan);
int cstatus(userrec *user, chanrec *chan);
std::string chlist(userrec *user, userrec* source);
int cflags(userrec *user, chanrec *chan);

#endif
