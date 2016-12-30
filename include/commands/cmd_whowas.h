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

namespace WhoWas
{
	/** One entry for a nick. There may be multiple entries for a nick.
	 */
	struct Entry
	{
		/** Real host
		 */
		const std::string host;

		/** Displayed host
		 */
		const std::string dhost;

		/** Ident
		 */
		const std::string ident;

		/** Server name
		 */
		const std::string server;

		/** Full name (GECOS)
		 */
		const std::string gecos;

		/** Signon time
		 */
		const time_t signon;

		/** Initialize this Entry with a user
		 */
		Entry(User* user);
	};

	/** Everything known about one nick
	 */
	struct Nick : public insp::intrusive_list_node<Nick>
	{
		/** A group of users related by nickname
		 */
		typedef std::deque<Entry*> List;

		/** Container where each element has information about one occurrence of this nick
		 */
		List entries;

		/** Time this nick was added to the database
		 */
		const time_t addtime;

		/** Nickname whose information is stored in this class
		 */
		const std::string nick;

		/** Constructor to initialize fields
		 */
		Nick(const std::string& nickname);

		/** Destructor, deallocates all elements in the entries container
		 */
		~Nick();
	};

	class Manager
	{
	 public:
		struct Stats
		{
			/** Number of currently existing WhoWas::Entry objects
			 */
			size_t entrycount;
		};

		/** Add a user to the whowas database. Called when a user quits.
		 * @param user The user to add to the database
		 */
		void Add(User* user);

		/** Retrieves statistics about the whowas database
		 * @return Whowas statistics as a WhoWas::Manager::Stats struct
		 */
		Stats GetStats() const;

		/** Expires old entries
		 */
		void Maintain();

		/** Updates the current configuration which may result in the database being pruned if the
		 * new values are lower than the current ones.
		 * @param NewGroupSize Maximum number of nicks allowed in the database. In case there are this many nicks
		 * in the database and one more is added, the oldest one is removed (FIFO).
		 * @param NewMaxGroups Maximum number of entries per nick
		 * @param NewMaxKeep Seconds how long each nick should be kept
		 */
		void UpdateConfig(unsigned int NewGroupSize, unsigned int NewMaxGroups, unsigned int NewMaxKeep);

		/** Retrieves all data known about a given nick
		 * @param nick Nickname to find, case insensitive (IRC casemapping)
		 * @return A pointer to a WhoWas::Nick if the nick was found, NULL otherwise
		 */
		const Nick* FindNick(const std::string& nick) const;

		/** Returns true if WHOWAS is enabled according to the current configuration
		 * @return True if WHOWAS is enabled according to the configuration, false if WHOWAS is disabled
		 */
		bool IsEnabled() const;

		/** Constructor
		 */
		Manager();

		/** Destructor
		 */
		~Manager();

	 private:
		/** Order in which the users were added into the map, used to remove oldest nick
		 */
		typedef insp::intrusive_list_tail<Nick> FIFO;

		/** Sets of users in the whowas system
		 */
		typedef TR1NS::unordered_map<std::string, WhoWas::Nick*, irc::insensitive, irc::StrHashComp> whowas_users;

		/** Primary container, links nicknames tracked by WHOWAS to a list of records
		 */
		whowas_users whowas;

		/** List of nicknames in the order they were inserted into the map
		 */
		FIFO whowas_fifo;

		/** Max number of WhoWas entries per user.
		 */
		unsigned int GroupSize;

		/** Max number of cumulative user-entries in WhoWas.
		 * When max reached and added to, push out oldest entry FIFO style.
		 */
		unsigned int MaxGroups;

		/** Max seconds a user is kept in WhoWas before being pruned.
		 */
		unsigned int MaxKeep;

		/** Shrink all data structures to honor the current settings
		 */
		void Prune();

		/** Remove a nick (and all entries belonging to it) from the database
		 * @param it Iterator to the nick to purge
		 */
		void PurgeNick(whowas_users::iterator it);

		/** Remove a nick (and all entries belonging to it) from the database
		 * @param nick Nick to purge
		 */
		void PurgeNick(WhoWas::Nick* nick);
	};
}

/** Handle /WHOWAS. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandWhowas : public Command
{
  public:
	/** Manager handling all whowas database related tasks
	 */
	WhoWas::Manager manager;

	CommandWhowas(Module* parent);
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};
