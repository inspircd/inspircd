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

#ifndef __COMMANDS_H
#define __COMMANDS_H

// include the common header files

#include <typeinfo>
#include <iostream>
#include <string>
#include <deque>
#include <sstream>
#include <vector>
#include "users.h"
#include "channels.h"


/** These are the handlers for user commands
 */
void handle_join(char **parameters, int pcnt, userrec *user);
void handle_part(char **parameters, int pcnt, userrec *user);
void handle_kick(char **parameters, int pcnt, userrec *user);
void handle_die(char **parameters, int pcnt, userrec *user);
void handle_restart(char **parameters, int pcnt, userrec *user);
void handle_kill(char **parameters, int pcnt, userrec *user);
void handle_summon(char **parameters, int pcnt, userrec *user);
void handle_users(char **parameters, int pcnt, userrec *user);
void handle_pass(char **parameters, int pcnt, userrec *user);
void handle_invite(char **parameters, int pcnt, userrec *user);
void handle_topic(char **parameters, int pcnt, userrec *user);
void handle_names(char **parameters, int pcnt, userrec *user);
void handle_privmsg(char **parameters, int pcnt, userrec *user);
void handle_notice(char **parameters, int pcnt, userrec *user);
void handle_info(char **parameters, int pcnt, userrec *user);
void handle_time(char **parameters, int pcnt, userrec *user);
void handle_whois(char **parameters, int pcnt, userrec *user);
void handle_quit(char **parameters, int pcnt, userrec *user);
void handle_who(char **parameters, int pcnt, userrec *user);
void handle_wallops(char **parameters, int pcnt, userrec *user);
void handle_list(char **parameters, int pcnt, userrec *user);
void handle_rehash(char **parameters, int pcnt, userrec *user);
void handle_lusers(char **parameters, int pcnt, userrec *user);
void handle_admin(char **parameters, int pcnt, userrec *user);
void handle_ping(char **parameters, int pcnt, userrec *user);
void handle_pong(char **parameters, int pcnt, userrec *user);
void handle_motd(char **parameters, int pcnt, userrec *user);
void handle_rules(char **parameters, int pcnt, userrec *user);
void handle_user(char **parameters, int pcnt, userrec *user);
void handle_userhost(char **parameters, int pcnt, userrec *user);
void handle_ison(char **parameters, int pcnt, userrec *user);
void handle_away(char **parameters, int pcnt, userrec *user);
void handle_whowas(char **parameters, int pcnt, userrec *user);
void handle_trace(char **parameters, int pcnt, userrec *user);
void handle_modules(char **parameters, int pcnt, userrec *user);
void handle_stats(char **parameters, int pcnt, userrec *user);
void handle_connect(char **parameters, int pcnt, userrec *user);
void handle_squit(char **parameters, int pcnt, userrec *user);
void handle_links(char **parameters, int pcnt, userrec *user);
void handle_map(char **parameters, int pcnt, userrec *user);
void handle_oper(char **parameters, int pcnt, userrec *user);
void handle_nick(char **parameters, int pcnt, userrec *user);
void handle_kline(char **parameters, int pcnt, userrec *user);
void handle_gline(char **parameters, int pcnt, userrec *user);
void handle_zline(char **parameters, int pcnt, userrec *user);
void handle_qline(char **parameters, int pcnt, userrec *user);
void handle_eline(char **parameters, int pcnt, userrec *user);
void handle_server(char **parameters, int pcnt, userrec *user);
void handle_loadmodule(char **parameters, int pcnt, userrec *user);
void handle_unloadmodule(char **parameters, int pcnt, userrec *user);
void handle_commands(char **parameters, int pcnt, userrec *user);
void handle_version(char **parameters, int pcnt, userrec *user);

/** Functions for u:lined servers
 */
bool is_uline(const char* server);

/** Other useful functions
 */
long duration(const char* str);

void do_whois(userrec* user, userrec* dest,unsigned long signon, unsigned long idle, char* nick);

#endif
