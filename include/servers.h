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
	
};



#endif

