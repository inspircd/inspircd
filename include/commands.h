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
bool host_matches_everyone(const std::string &mask, userrec* user);
bool ip_matches_everyone(const std::string &ip, userrec* user);
bool nick_matches_everyone(const std::string &nick, userrec* user);	
int operstrcmp(char* data,char* input);
void split_chlist(userrec* user, userrec* dest, const std::string &cl);

/*       XXX Serious WTFness XXX
 *
 * Well, unless someone invents a wildcard or
 * regexp #include, and makes it a standard,
 * we're stuck with this way of including all
 * the commands.
 */

#include "commands/cmd_admin.h"
#include "commands/cmd_away.h"
#include "commands/cmd_commands.h"
#include "commands/cmd_connect.h"
#include "commands/cmd_die.h"
#include "commands/cmd_eline.h"
#include "commands/cmd_gline.h"
#include "commands/cmd_info.h"
#include "commands/cmd_invite.h"
#include "commands/cmd_ison.h"
#include "commands/cmd_join.h"
#include "commands/cmd_kick.h"
#include "commands/cmd_kill.h"
#include "commands/cmd_kline.h"
#include "commands/cmd_links.h"
#include "commands/cmd_list.h"
#include "commands/cmd_loadmodule.h"
#include "commands/cmd_lusers.h"
#include "commands/cmd_map.h"
#include "commands/cmd_modules.h"
#include "commands/cmd_motd.h"
#include "commands/cmd_names.h"
#include "commands/cmd_nick.h"
#include "commands/cmd_notice.h"
#include "commands/cmd_oper.h"
#include "commands/cmd_part.h"
#include "commands/cmd_pass.h"
#include "commands/cmd_ping.h"
#include "commands/cmd_pong.h"
#include "commands/cmd_privmsg.h"
#include "commands/cmd_qline.h"
#include "commands/cmd_quit.h"
#include "commands/cmd_rehash.h"
#include "commands/cmd_restart.h"
#include "commands/cmd_rules.h"
#include "commands/cmd_server.h"
#include "commands/cmd_squit.h"
#include "commands/cmd_stats.h"
#include "commands/cmd_summon.h"
#include "commands/cmd_time.h"
#include "commands/cmd_topic.h"
#include "commands/cmd_trace.h"
#include "commands/cmd_unloadmodule.h"
#include "commands/cmd_user.h"
#include "commands/cmd_userhost.h"
#include "commands/cmd_users.h"
#include "commands/cmd_version.h"
#include "commands/cmd_wallops.h"
#include "commands/cmd_who.h"
#include "commands/cmd_whois.h"
#include "commands/cmd_whowas.h"
#include "commands/cmd_zline.h"


#endif
