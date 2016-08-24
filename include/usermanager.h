/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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

#include <list>

class CoreExport UserManager : public fakederef<UserManager>
{
 public:
	struct CloneCounts
	{
		unsigned int global;
		unsigned int local;
		CloneCounts() : global(0), local(0) { }
	};

	/** Container that maps IP addresses to clone counts
	 */
	typedef std::map<irc::sockets::cidr_mask, CloneCounts> CloneMap;

	/** Sequence container in which each element is a User*
	 */
	typedef std::vector<User*> OperList;

	/** A list holding local users
	*/
	typedef insp::intrusive_list<LocalUser> LocalList;

 private:
	/** Map of IP addresses for clone counting
	 */
	CloneMap clonemap;

	/** A CloneCounts that contains zero for both local and global
	 */
	const CloneCounts zeroclonecounts;

	/** Local client list, a list containing only local clients
	 */
	LocalList local_users;

	/** Last used already sent id, used when sending messages to neighbors to help determine whether the message has
	 * been sent to a particular user or not. See User::ForEachNeighbor() for more info.
	 */
	already_sent_t already_sent_id;

 public:
	/** Constructor, initializes variables
	 */
	UserManager();

	/** Destructor, destroys all users in clientlist
	 */
	~UserManager();

	/** Nickname string -> User* map. Contains all users, including unregistered ones.
	 */
	user_hash clientlist;

	/** UUID -> User* map. Contains all users, including unregistered ones.
	 */
	user_hash uuidlist;

	/** Oper list, a vector containing all local and remote opered users
	 */
	OperList all_opers;

	/** Number of unregistered users online right now.
	 * (Unregistered means before USER/NICK/dns)
	 */
	unsigned int unregistered_count;

	/** Perform background user events for all local users such as PING checks, registration timeouts,
	 * penalty management and recvq processing for users who have data in their recvq due to throttling.
	 */
	void DoBackgroundUserStuff();

	/** Returns true when all modules have done pre-registration checks on a user
	 * @param user The user to verify
	 * @return True if all modules have finished checking this user
	 */
	bool AllModulesReportReady(LocalUser* user);

	/** Handle a client connection.
	 * Creates a new LocalUser object, inserts it into the appropriate containers,
	 * initializes it as not yet registered, and adds it to the socket engine.
	 *
	 * The new user may immediately be quit after being created, for example if the user limit
	 * is reached or if the user is banned.
	 * @param socket File descriptor of the connection
	 * @param via Listener socket that this user connected to
	 * @param client The IP address and client port of the user
	 * @param server The server IP address and port used by the user
	 */
	void AddUser(int socket, ListenSocket* via, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server);

	/** Disconnect a user gracefully.
	 * When this method returns the user provided will be quit, but the User object will continue to be valid and will be deleted at the end of the current main loop iteration.
	 * @param user The user to remove
	 * @param quitreason The quit reason to show to normal users
	 * @param operreason The quit reason to show to opers, can be NULL if same as quitreason
	 */
	void QuitUser(User* user, const std::string& quitreason, const std::string* operreason = NULL);

	/** Add a user to the clone map
	 * @param user The user to add
	 */
	void AddClone(User* user);

	/** Remove all clone counts from the user, you should
	 * use this if you change the user's IP address
	 * after they have registered.
	 * @param user The user to remove
	 */
	void RemoveCloneCounts(User *user);

	/** Rebuild clone counts. Required when <cidr> settings change.
	 */
	void RehashCloneCounts();

	/** Return the number of local and global clones of this user
	 * @param user The user to get the clone counts for
	 * @return The clone counts of this user. The returned reference is volatile - you
	 * must assume that it becomes invalid as soon as you call any function other than
	 * your own.
	 */
	const CloneCounts& GetCloneCounts(User* user) const;

	/** Return a map containg IP addresses and their clone counts
	 * @return The clone count map
	 */
	const CloneMap& GetCloneMap() const { return clonemap; }

	/** Return a count of all global users, unknown and known connections
	 * @return The number of users on the network, including local unregistered users
	 */
	unsigned int UserCount() const { return this->clientlist.size(); }

	/** Return a count of fully registered connections on the network
	 * @return The number of registered users on the network
	 */
	unsigned int RegisteredUserCount() { return this->clientlist.size() - this->UnregisteredUserCount(); }

	/** Return a count of opered (umode +o) users on the network
	 * @return The number of opers on the network
	 */
	unsigned int OperCount() const { return this->all_opers.size(); }

	/** Return a count of local unregistered (before NICK/USER) users
	 * @return The number of local unregistered (unknown) connections
	 */
	unsigned int UnregisteredUserCount() const { return this->unregistered_count; }

	/** Return a count of local registered users
	 * @return The number of registered local users
	 */
	unsigned int LocalUserCount() const { return (this->local_users.size() - this->UnregisteredUserCount()); }

	/** Get a hash map containing all users, keyed by their nickname
	 * @return A hash map mapping nicknames to User pointers
	 */
	user_hash& GetUsers() { return clientlist; }

	/** Get a list containing all local users
	 * @return A const list of local users
	 */
	const LocalList& GetLocalUsers() const { return local_users; }

	/** Send a server notice to all local users
	 * @param text The text format string to send
	 * @param ... The format arguments
	 */
	void ServerNoticeAll(const char* text, ...) CUSTOM_PRINTF(2, 3);

	/** Retrieves the next already sent id, guaranteed to be not equal to any user's already_sent field
	 * @return Next already_sent id
	 */
	already_sent_t NextAlreadySentId();
};
