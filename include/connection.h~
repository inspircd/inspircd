/*

$Log$
Revision 1.1  2003/01/26 23:52:59  brain
Modified documentation for base classes
Added base classes

Revision 1.1  2003/01/26 20:15:00  brain
Added server classes for linking


*/

#include "inspircd_config.h" 
#include "base.h"
#include <string>
#include <map.h>
 
#ifndef __CONNECTION_H__ 
#define __CONNECTION_H__ 
 
class connection : public classbase
{
 public:
	int fd;			// file descriptor
	char host[256];		// hostname
	long ip;		// ipv4 address
	char inbuf[MAXBUF];	// recvQ
	long bytes_in;
	long bytes_out;
	long cmds_in;
	long cmds_out;
	bool haspassed;
	int port;
	int registered;
	time_t lastping;
	time_t signon;
	time_t idle_lastmsg;
	time_t nping;
};


#endif

