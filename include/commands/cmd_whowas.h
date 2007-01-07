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

#ifndef __CMD_WHOWAS_H__
#define __CMD_WHOWAS_H__


// include the common header files

#include "users.h"
#include "channels.h"

class MaintainTimer;

/** InspTimer that is used to maintain the whowas list, called once an hour
 */
MaintainTimer* timer;

/** Handle /WHOWAS
 */
class cmd_whowas : public command_t
{
  public:
	cmd_whowas(InspIRCd* Instance);
	CmdResult Handle(const char** parameters, int pcnt, userrec *user);
	CmdResult HandleInternal(const unsigned int id, const std::deque<classbase*> &parameters);
	void AddToWhoWas(userrec* user);
	void GetStats(Extensible* ext);
	void PruneWhoWas(time_t t);
	virtual ~cmd_whowas();
};

enum Internals
{
	WHOWAS_ADD = 1,
	WHOWAS_STATS = 2,
	WHOWAS_PRUNE = 3
};


/** Used to hold WHOWAS information
 */
class WhoWasGroup : public classbase
{
 public:
	/** Real host
	 */
	char* host;
	/** Displayed host
	 */
	char* dhost;
	/** Ident
	 */
	char* ident;
	/** Server name
	 */
	const char* server;
	/** Fullname (GECOS)
	 */
	char* gecos;
	/** Signon time
	 */
	time_t signon;

	/** Initialize this WhoQasFroup with a user
	 */
	WhoWasGroup(userrec* user);
	/** Destructor
	 */
	~WhoWasGroup();
};

class MaintainTimer : public InspTimer
{
  private:
	InspIRCd* ServerInstance;
  public:
	MaintainTimer(InspIRCd* Instance, long interval)
	: InspTimer(interval, Instance->Time()), ServerInstance(Instance)
	{
	}
	virtual void Tick(time_t TIME);
};

/** A group of users related by nickname
 */
typedef std::deque<WhoWasGroup*> whowas_set;

/** Sets of users in the whowas system
 */
typedef std::map<irc::string,whowas_set*> whowas_users;

/** Sets of time and users in whowas list
 */
typedef std::deque<std::pair<time_t,irc::string> > whowas_users_fifo;

/** Called every hour by the core to remove expired entries
 */
void MaintainWhoWas(InspIRCd* ServerInstance, time_t TIME);

/** Whowas container, contains a map of vectors of users tracked by WHOWAS
 */
whowas_users whowas;

/** Whowas container, contains a map of time_t to users tracked by WHOWAS
 */
whowas_users_fifo whowas_fifo;

#endif
