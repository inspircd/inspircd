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

#ifndef __MODE_H
#define __MODE_H

// include the common header files

#include <typeinfo>
#include <iostream>
#include <string>
#include <deque>
#include <sstream>
#include <vector>
#include "users.h"
#include "channels.h"


char* give_ops(userrec *user,char *dest,chanrec *chan,int status);
char* give_hops(userrec *user,char *dest,chanrec *chan,int status);
char* give_voice(userrec *user,char *dest,chanrec *chan,int status);
char* take_ops(userrec *user,char *dest,chanrec *chan,int status);
char* take_hops(userrec *user,char *dest,chanrec *chan,int status);
char* take_voice(userrec *user,char *dest,chanrec *chan,int status);
char* add_ban(userrec *user,char *dest,chanrec *chan,int status);
char* take_ban(userrec *user,char *dest,chanrec *chan,int status);
void process_modes(char **parameters,userrec* user,chanrec *chan,int status, int pcnt, bool servermode, bool silent, bool local);
bool allowed_umode(char umode, char* sourcemodes,bool adding);
bool process_module_umode(char umode, userrec* source, void* dest, bool adding);
void handle_mode(char **parameters, int pcnt, userrec *user);
void server_mode(char **parameters, int pcnt, userrec *user);
void merge_mode(char **parameters, int pcnt);
void merge_mode2(char **parameters, int pcnt, userrec* user);


#endif
