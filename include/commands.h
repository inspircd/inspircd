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


/** Special functions for processing server to server traffic
 */
void handle_link_packet(char* udp_msg, char* tcp_host, serverrec *serv);
void process_restricted_commands(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host,char* ipaddr,int port);

/** These are the handlers for server commands (tokens)
 */
void handle_amp(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host);
void handle_dollar(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host);
void handle_J(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host);
void handle_R(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host);
void handle_plus(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host);
void handle_b(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host);
void handle_a(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host);
void handle_F(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host);
void handle_N(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host);
void handle_AT(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host);
void handle_k(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host);
void handle_n(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host);
void handle_Q(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host);
void handle_K(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host);
void handle_L(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host);
void handle_m(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host);
void handle_M(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host);
void handle_T(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host);
void handle_t(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host);
void handle_i(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host);
void handle_P(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host);
void handle_V(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host);

/** Functions for u:lined servers
 */
bool is_uline(const char* server);

#endif
