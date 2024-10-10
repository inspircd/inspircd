/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012-2013, 2017, 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

/** A mapping of user nicks or uuids to their User object. */
typedef std::unordered_map<std::string, User*, irc::insensitive, irc::StrHashComp> UserMap;

class CoreExport UserManager final
{
public:
	struct CloneCounts final
	{
		unsigned int global = 0;
		unsigned int local = 0;
	};

	/** Container that maps IP addresses to clone counts
	 */
	typedef std::map<irc::sockets::cidr_mask, CloneCounts> CloneMap;

	/** Sequence container in which each element is a User*
	 */
	typedef std::vector<User*> OperList;

	/** A list containing users who are on a services server. */
	typedef std::vector<User*> ServiceList;

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
	uint64_t already_sent_id = 0;

public:
	/** Constructor, initializes variables
	 */
	UserManager();

	/** Destructor, destroys all users in clientlist
	 */
	~UserManager();

	/** Nickname string -> User* map. Contains all users, including partially connected ones.
	 */
	UserMap clientlist;

	/** UUID -> User* map. Contains all users, including partially connected ones.
	 */
	UserMap uuidlist;

	/** Oper list, a vector containing all local and remote opered users
	 */
	OperList all_opers;

	/** A list of users on services servers. */
	ServiceList all_services;

	/** Number of local unknown (not fully connected) users. */
	size_t unknown_count = 0;

	/** Perform background user events for all local users such as PING checks, connection timeouts,
	 * penalty management and recvq processing for users who have data in their recvq due to throttling.
	 */
	void DoBackgroundUserStuff();

	/** Handle a client connection.
	 * Creates a new LocalUser object, inserts it into the appropriate containers,
	 * initializes it as not fully connected, and adds it to the socket engine.
	 *
	 * The new user may immediately be quit after being created, for example if the user limit
	 * is reached or if the user is banned.
	 * @param socket File descriptor of the connection
	 * @param via Listener socket that this user connected to
	 * @param client The IP address and client port of the user
	 * @param server The server IP address and port used by the user
	 */
	void AddUser(int socket, ListenSocket* via, const irc::sockets::sockaddrs& client, const irc::sockets::sockaddrs& server) ATTR_NOT_NULL(3);

	/** Disconnect a user gracefully.
	 * When this method returns the user provided will be quit, but the User object will continue to be valid and will be deleted at the end of the current main loop iteration.
	 * @param user The user to remove
	 * @param quitreason The quit reason to show to normal users
	 * @param operreason The quit reason to show to opers, can be NULL if same as quitreason
	 */
	void QuitUser(User* user, const std::string& quitreason, const std::string* operreason = nullptr) ATTR_NOT_NULL(2);

	/** Add a user to the clone map
	 * @param user The user to add
	 */
	void AddClone(User* user) ATTR_NOT_NULL(2);

	/** Remove all clone counts from the user, you should
	 * use this if you change the user's IP address
	 * after they have fully connected.
	 * @param user The user to remove
	 */
	void RemoveCloneCounts(User* user) ATTR_NOT_NULL(2);

	/** Rebuild clone counts. Required when \<cidr> settings change.
	 */
	void RehashCloneCounts();

	/** Rebuilds the list of services servers. Required when \<services> settings change. */
	void RehashServices();

	/** Return the number of local and global clones of this user
	 * @param user The user to get the clone counts for
	 * @return The clone counts of this user. The returned reference is volatile - you
	 * must assume that it becomes invalid as soon as you call any function other than
	 * your own.
	 */
	const CloneCounts& GetCloneCounts(User* user) const ATTR_NOT_NULL(2);

	/** Return a map containing IP addresses and their clone counts
	 * @return The clone count map
	 */
	const CloneMap& GetCloneMap() const { return clonemap; }

	/** Return a count of local unknown (not fully connected) users.
	 * @return The number of local unknown (not fully connected) users.
	 */
	size_t UnknownUserCount() const { return this->unknown_count; }

	/** Return a count of users on a services servers.
	 * @return The number of users on services servers.
	 */
	size_t ServiceCount() const { return this->all_services.size(); }

	/** Return a count of fully connected connections on this server.
	 * @return The number of fully connected users on this server.
	 */
	size_t LocalUserCount() const { return this->local_users.size() - this->UnknownUserCount(); }

	/** Return a count of fully connected connections on the network.
	 * @return The number of fully connected users on the network.
	 */
	size_t GlobalUserCount() const { return this->clientlist.size() - this->UnknownUserCount() - this->ServiceCount(); }

	/** Get a hash map containing all users, keyed by their nickname
	 * @return A hash map mapping nicknames to User pointers
	 */
	UserMap& GetUsers() { return clientlist; }

	/** Get a list containing all local users
	 * @return A const list of local users
	 */
	const LocalList& GetLocalUsers() const { return local_users; }

	/** Retrieves the next already sent id, guaranteed to be not equal to any user's already_sent field
	 * @return Next already_sent id
	 */
	uint64_t NextAlreadySentId();

	/** Find a user by their nickname or UUID.
	 * IMPORTANT: You probably want to use FindNick or FindUUID instead of this.
	 * @param nickuuid The nickname or UUID of the user to find.
	 * @param fullyconnected Whether to only return users who are fully connected to the server.
	 * @return If the user was found then a pointer to a User object; otherwise, nullptr.
	 */
	User* Find(const std::string& nickuuid, bool fullyconnected = false);

	/** Find a local user by their nickname or UUID.
	 * IMPORTANT: You probably want to use FindNick or FindUUID instead of this.
	 * @param nickuuid The nickname or UUID of the user to find.
	 * @param fullyconnected Whether to only return users who are fully connected to the server.
	 * @return If the user was found then a pointer to a User object; otherwise, nullptr.
	 */
	template<typename T>
	std::enable_if_t<std::is_same_v<T, LocalUser>, T*> Find(const std::string& nickuuid, bool fullyconnected = false)
	{
		return IS_LOCAL(Find(nickuuid, fullyconnected));
	}

	/** Find a remote user by their nickname or UUID.
	 * IMPORTANT: You probably want to use FindNick or FindUUID instead of this.
	 * @param nickuuid The nickname or UUID of the user to find.
	 * @param fullyconnected Whether to only return users who are fully connected to the server.
	 * @return If the user was found then a pointer to a User object; otherwise, nullptr.
	 */
	template<typename T>
	std::enable_if_t<std::is_same_v<T, RemoteUser>, T*> Find(const std::string& nickuuid, bool fullyconnected = false)
	{
		return IS_REMOTE(Find(nickuuid, fullyconnected));
	}

	/** Find a user by their nickname.
	 * @param nick The nickname of the user to find.
	 * @param fullyconnected Whether to only return users who are fully connected to the server.
	 * @return If the user was found then a pointer to a User object; otherwise, nullptr.
	 */
	User* FindNick(const std::string& nick, bool fullyconnected = false);

	/** Find a local user by their nickname.
	 * @param nick The nickname of the user to find.
	 * @param fullyconnected Whether to only return users who are fully connected to the server.
	 * @return If the user was found then a pointer to a User object; otherwise, nullptr.
	 */
	template<typename T>
	std::enable_if_t<std::is_same_v<T, LocalUser>, T*> FindNick(const std::string& nick, bool fullyconnected = false)
	{
		return IS_LOCAL(FindNick(nick, fullyconnected));
	}

	/** Find a remote user by their nickname.
	 * @param nick The nickname of the user to find.
	 * @param fullyconnected Whether to only return users who are fully connected to the server.
	 * @return If the user was found then a pointer to a User object; otherwise, nullptr.
	 */
	template<typename T>
	std::enable_if_t<std::is_same_v<T, RemoteUser>, T*> FindNick(const std::string& nick, bool fullyconnected = false)
	{
		return IS_REMOTE(FindNick(nick, fullyconnected));
	}

	/** Find a user by their UUID.
	 * @param uuid The UUID of the user to find.
	 * @param fullyconnected Whether to only return users who are fully connected to the server.
	 * @return If the user was found then a pointer to a User object; otherwise, nullptr.
	 */
	User* FindUUID(const std::string& uuid, bool fullyconnected = false);

	/** Find a local user by their UUID.
	 * @param uuid The UUID of the user to find.
	 * @param fullyconnected Whether to only return users who are fully connected to the server.
	 * @return If the user was found then a pointer to a User object; otherwise, nullptr.
	 */
	template<typename T>
	std::enable_if_t<std::is_same_v<T, LocalUser>, T*> FindUUID(const std::string& uuid, bool fullyconnected = false)
	{
		return IS_LOCAL(FindUUID(uuid, fullyconnected));
	}

	/** Find a remote user by their UUID.
	 * @param uuid The UUID of the user to find.
	 * @param fullyconnected Whether to only return users who are fully connected to the server.
	 * @return If the user was found then a pointer to a User object; otherwise, nullptr.
	 */
	template<typename T>
	std::enable_if_t<std::is_same_v<T, RemoteUser>, T*> FindUUID(const std::string& uuid, bool fullyconnected = false)
	{
		return IS_REMOTE(FindUUID(uuid, fullyconnected));
	}
};
