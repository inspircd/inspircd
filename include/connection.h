/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
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
#include "base.h"
#include <string>
#include <map>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <vector>
#include <deque>
#include <sstream>

#ifndef __CONNECTION_H__
#define __CONNECTION_H__

/** Please note: classes serverrec and userrec both inherit from class connection.
 */
class connection : public Extensible
{
 public:
	/** File descriptor of the connection
	 */
	int fd;
	
	/** Hostname of connection. Not used if this is a serverrec
	 */
	char host[65];

	/** Stats counter for bytes inbound
	 */
	int bytes_in;

	/** Stats counter for bytes outbound
	 */
	int bytes_out;

	/** Stats counter for commands inbound
	 */
	int cmds_in;

	/** Stats counter for commands outbound
	 */
	int cmds_out;

	/** True if server/user has authenticated, false if otherwise
	 */
	bool haspassed;

	/** Port number
	 * For a userrec, this is the port they connected to the network on.
	 * For a serverrec this is the current listening port of the serverrec object.
	 */
	int port;
	
	/** Used by userrec to indicate the registration status of the connection
	 */
	char registered;
	
	/** Time the connection was last pinged
	 */
	time_t lastping;
	
	/** Time the connection was created, set in the constructor
	 */
	time_t signon;
	
	/** Time that the connection last sent data, used to calculate idle time
	 */
	time_t idle_lastmsg;
	
	/** Used by PING checks with clients
	 */
	time_t nping;
	
	/** Default constructor
	 */
	connection()
	{
		this->fd = -1;
	}
};


#endif


