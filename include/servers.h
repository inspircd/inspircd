/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
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

#include "inspircd_config.h" 
#include "connection.h"
#include <string>
#include <map>
 
#ifndef __SERVERS_H__ 
#define __SERVERS_H__ 
 
#define LINK_ACTIVE	1
#define LINK_INACTIVE	0

/** A class that defines the local server or a remote server
 */
class serverrec : public connection
{
 private:
 public:
	/** server name
	 */
	char name[MAXBUF];
	/** last ping response (ms)
	 */
	long pingtime;
	/** invisible users on server
	 */
	long usercount_i;
	/** non-invisible users on server
	 */
	long usercount;
	/** opers on server
	 */
	long opercount;
	/** number of hops away (for quick access)
	 */
	int hops_away;
	/** ircd version
	 */
	long version;
	/** is a JUPE server (faked to enforce a server ban)
	 */
	bool jupiter;
	
	/** Description of the server
	 */	
	char description[MAXBUF];

	/** Holds nickserv's name on U:lined (services) servers (this is a kludge for ircservices which ASSUMES things :/)
	 */
	char nickserv[NICKMAX];
	
	bool sync_soon;

	/** Constructor
	 */
	serverrec();
	/** Constructor which initialises some of the main variables
	 */
	serverrec(char* n, long ver, bool jupe);
	/** Destructor
	 */
	~serverrec();

        /** With a serverrec, this is a list of all established server connections.
         */
        std::vector<ircd_connector> connectors;


        /** Create a listening socket on 'host' using port number 'p'
         */
        bool CreateListener(char* host, int p);

        /** Begin an outbound link to another ircd at targethost.
         */
        bool BeginLink(char* targethost, int port, char* password, char* servername, int myport);

        /** Begin an outbound mesh link to another ircd on a network you are already an authenticated member of
         */
        bool MeshCookie(char* targethost, int port, unsigned long cookie, char* servername);

        /** Terminate a link to 'targethost' by calling the ircd_connector::CloseConnection method.
         */
        void TerminateLink(char* targethost);

        /** Send a message to a server by name, if the server is unavailable directly route the packet via another server
         * If the server still cannot be reached after attempting to route the message remotely, returns false.
         */
        bool SendPacket(char *message, const char* host);

        /** Returns the next available packet and returns true if data is available. Writes the servername the data came from to 'host'.
         * If no data is available this function returns false.
         * This function will automatically close broken links and reroute pathways, generating split messages on the network.
         */
        bool RecvPacket(std::deque<std::string> &messages, char* host, std::deque<std::string> &sums);

        /** Find the ircd_connector oject related to a certain servername given in 'host'
         */
        ircd_connector* FindHost(std::string host);

        /** Add an incoming connection to the connection pool.
         * (reserved for core use)
         */
        bool AddIncoming(int fd,char* targethost, int sourceport);	

	/** Flushes all data waiting to be written for all of this server's connections
	 */
	void FlushWriteBuffers();
};

#endif

