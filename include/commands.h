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

bool is_uline(const char* server);
long duration(const char* str);
void do_whois(userrec* user, userrec* dest,unsigned long signon, unsigned long idle, char* nick);
bool host_matches_everyone(std::string mask, userrec* user);
bool ip_matches_everyone(std::string ip, userrec* user);
bool nick_matches_everyone(std::string nick, userrec* user);	
int operstrcmp(char* data,char* input);

/*       XXX Serious WTFness XXX
 *
 * Well, unless someone invents a wildcard or
 * regexp #include, and makes it a standard,
 * we're stuck with this way of including all
 * the commands.
 */

#include "cmd_admin.h"
#include "cmd_away.h"
#include "cmd_commands.h"
#include "cmd_connect.h"
#include "cmd_die.h"
#include "cmd_eline.h"
#include "cmd_gline.h"
#include "cmd_info.h"
#include "cmd_invite.h"
#include "cmd_ison.h"
#include "cmd_join.h"
#include "cmd_kick.h"
#include "cmd_kill.h"
#include "cmd_kline.h"
#include "cmd_links.h"
#include "cmd_list.h"
#include "cmd_loadmodule.h"
#include "cmd_lusers.h"
#include "cmd_map.h"
#include "cmd_modules.h"
#include "cmd_motd.h"
#include "cmd_names.h"
#include "cmd_nick.h"
#include "cmd_notice.h"
#include "cmd_oper.h"
#include "cmd_part.h"
#include "cmd_pass.h"
#include "cmd_ping.h"
#include "cmd_pong.h"
#include "cmd_privmsg.h"
#include "cmd_qline.h"
#include "cmd_quit.h"
#include "cmd_rehash.h"
#include "cmd_restart.h"
#include "cmd_rules.h"
#include "cmd_server.h"
#include "cmd_squit.h"
#include "cmd_stats.h"
#include "cmd_summon.h"
#include "cmd_time.h"
#include "cmd_topic.h"
#include "cmd_trace.h"
#include "cmd_unloadmodule.h"
#include "cmd_user.h"
#include "cmd_userhost.h"
#include "cmd_users.h"
#include "cmd_version.h"
#include "cmd_wallops.h"
#include "cmd_who.h"
#include "cmd_whois.h"
#include "cmd_whowas.h"
#include "cmd_zline.h"


#endif
