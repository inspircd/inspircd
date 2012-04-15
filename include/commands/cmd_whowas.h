/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *      the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef CMD_WHOWAS_H
#define CMD_WHOWAS_H
#include "modules.h"

struct WhowasRequest : public Request
{
	/* list of available internal commands */
	enum Internals
	{
		WHOWAS_ADD = 1,
		WHOWAS_STATS = 2,
		WHOWAS_PRUNE = 3,
		WHOWAS_MAINTAIN = 4
	};

	const Internals type;
	std::string value;
	User* user;

	WhowasRequest(Module* src, Module* whowas, Internals Type) : Request(src, whowas, "WHOWAS"), type(Type)
	{}
};

/* Forward ref for timer */
class WhoWasMaintainTimer;

/* Forward ref for typedefs */
class WhoWasGroup;

/** Timer that is used to maintain the whowas list, called once an hour
 */
extern WhoWasMaintainTimer* timer;

/** A group of users related by nickname
 */
typedef std::deque<WhoWasGroup*> whowas_set;

/** Sets of users in the whowas system
 */
typedef std::map<irc::string,whowas_set*> whowas_users;

/** Sets of time and users in whowas list
 */
typedef std::deque<std::pair<time_t,irc::string> > whowas_users_fifo;

/** Handle /WHOWAS. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandWhowas : public Command
{
  private:
	/** Whowas container, contains a map of vectors of users tracked by WHOWAS
	 */
	whowas_users whowas;

	/** Whowas container, contains a map of time_t to users tracked by WHOWAS
	 */
	whowas_users_fifo whowas_fifo;

  public:
	CommandWhowas(Module* parent);
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
	void AddToWhoWas(User* user);
	std::string GetStats();
	void PruneWhoWas(time_t t);
	void MaintainWhoWas(time_t t);
	~CommandWhowas();
};

/** Used to hold WHOWAS information
 */
class WhoWasGroup
{
 public:
	/** Real host
	 */
	std::string host;
	/** Displayed host
	 */
	std::string dhost;
	/** Ident
	 */
	std::string ident;
	/** Server name
	 */
	std::string server;
	/** Fullname (GECOS)
	 */
	std::string gecos;
	/** Signon time
	 */
	time_t signon;

	/** Initialize this WhoWasFroup with a user
	 */
	WhoWasGroup(User* user);
	/** Destructor
	 */
	~WhoWasGroup();
};

class WhoWasMaintainTimer : public Timer
{
  public:
	WhoWasMaintainTimer(long interval)
	: Timer(interval, ServerInstance->Time(), true)
	{
	}
	virtual void Tick(time_t TIME);
};

#endif
