/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#pragma once

#include "modules.h"

/* Forward ref for typedefs */
class WhoWasGroup;

/** A group of users related by nickname
 */
typedef std::deque<WhoWasGroup*> whowas_set;

namespace WhoWas
{
	/** Everything known about one nick
	 */
	struct Nick : public intrusive_list_node<Nick>
	{
		/** Container where each element has information about one occurrence of this nick
		 */
		whowas_set entries;

		/** Time this nick was added to the database
		 */
		const time_t addtime;

		/** Nickname whose information is stored in this class
		 */
		const irc::string nick;

		/** Constructor to initialize fields
		 */
		Nick(const irc::string& nickname);

		/** Destructor, deallocates all elements in the entries container
		 */
		~Nick();
	};

	/** Order in which the users were added into the map, used to remove oldest nick
	 */
	typedef intrusive_list_tail<Nick> FIFO;
}

/** Sets of users in the whowas system
 */
typedef std::map<irc::string, WhoWas::Nick*> whowas_users;

/** Handle /WHOWAS. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandWhowas : public Command
{
  private:
	/** Primary container, links nicknames tracked by WHOWAS to a list of records
	 */
	whowas_users whowas;

	/** List of nicknames in the order they were inserted into the map
	 */
	WhoWas::FIFO whowas_fifo;

  public:
	/** Max number of WhoWas entries per user.
	 */
	unsigned int GroupSize;

	/** Max number of cumulative user-entries in WhoWas.
	 *  When max reached and added to, push out oldest entry FIFO style.
	 */
	unsigned int MaxGroups;

	/** Max seconds a user is kept in WhoWas before being pruned.
	 */
	unsigned int MaxKeep;

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
	void Prune();
	void Maintain();
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
};
