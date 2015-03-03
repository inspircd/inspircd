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


#ifndef USERMANAGER_H
#define USERMANAGER_H

#include <list>

/** A list of ip addresses cross referenced against clone counts */
typedef std::map<irc::sockets::cidr_mask, unsigned int> clonemap;

class CoreExport UserManager
{
 private:
	/** Map of local ip addresses for clone counting
	 */
	clonemap local_clones;
 public:
	UserManager();

	~UserManager()
	{
		for (user_hash::iterator i = clientlist->begin();i != clientlist->end();i++)
		{
			delete i->second;
		}
		clientlist->clear();
		delete clientlist;
		delete uuidlist;
	}

	/** Client list, a hash_map containing all clients, local and remote
	 */
	user_hash* clientlist;

	/** Client list stored by UUID. Contains all clients, and is updated
	 * automatically by the constructor and destructor of User.
	 */
	user_hash* uuidlist;

	/** Local client list, a list containing only local clients
	 */
	LocalUserList local_users;

	/** Oper list, a vector containing all local and remote opered users
	 */
	std::list<User*> all_opers;

	/** Number of unregistered users online right now.
	 * (Unregistered means before USER/NICK/dns)
	 */
	unsigned int unregistered_count;

	/** Number of elements in local_users
	 */
	unsigned int local_count;

	/** Map of global ip addresses for clone counting
	 * XXX - this should be private, but m_clones depends on it currently.
	 */
	clonemap global_clones;

	/** Add a client to the system.
	 * This will create a new User, insert it into the user_hash,
	 * initialize it as not yet registered, and add it to the socket engine.
	 * @param socket The socket id (file descriptor) this user is on
	 * @param via The socket that this user connected using
	 * @param client The IP address and client port of the user
	 * @param server The server IP address and port used by the user
	 * @return This function has no return value, but a call to AddClient may remove the user.
	 */
	void AddUser(int socket, ListenSocket* via, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server);

	/** Disconnect a user gracefully
	 * @param user The user to remove
	 * @param quitreason The quit reason to show to normal users
	 * @param operreason The quit reason to show to opers
	 * @return Although this function has no return type, on exit the user provided will no longer exist.
	 */
	void QuitUser(User *user, const std::string &quitreason, const char* operreason = "");

	/** Add a user to the local clone map
	 * @param user The user to add
	 */
	void AddLocalClone(User *user);

	/** Add a user to the global clone map
	 * @param user The user to add
	 */
	void AddGlobalClone(User *user);

	/** Remove all clone counts from the user, you should
	 * use this if you change the user's IP address
	 * after they have registered.
	 * @param user The user to remove
	 */
	void RemoveCloneCounts(User *user);

	/** Rebuild clone counts
	 */
	void RehashCloneCounts();

	/** Return the number of global clones of this user
	 * @param user The user to get a count for
	 * @return The global clone count of this user
	 */
	unsigned long GlobalCloneCount(User *user);

	/** Return the number of local clones of this user
	 * @param user The user to get a count for
	 * @return The local clone count of this user
	 */
	unsigned long LocalCloneCount(User *user);

	/** Return a count of users, unknown and known connections
	 * @return The number of users
	 */
	unsigned int UserCount();

	/** Return a count of fully registered connections only
	 * @return The number of registered users
	 */
	unsigned int RegisteredUserCount();

	/** Return a count of opered (umode +o) users only
	 * @return The number of opers
	 */
	unsigned int OperCount();

	/** Return a count of unregistered (before NICK/USER) users only
	 * @return The number of unregistered (unknown) connections
	 */
	unsigned int UnregisteredUserCount();

	/** Return a count of local users on this server only
	 * @return The number of local users
	 */
	unsigned int LocalUserCount();




	/** Number of users with a certain mode set on them
	 */
	int ModeCount(const char mode);

	/** Send a server notice to all local users
	 * @param text The text format string to send
	 * @param ... The format arguments
	 */
	void ServerNoticeAll(const char* text, ...) CUSTOM_PRINTF(2, 3);

	/** Send a server message (PRIVMSG) to all local users
	 * @param text The text format string to send
	 * @param ... The format arguments
	 */
	void ServerPrivmsgAll(const char* text, ...) CUSTOM_PRINTF(2, 3);
};

#endif
