/*


*/

#include "inspircd_config.h" 
#include "connection.h"
#include <string>
#include <map.h>
 
#ifndef __SERVERS_H__ 
#define __SERVERS_H__ 
 
#define LINK_ACTIVE	1
#define LINK_INACTIVE	0

class serverrec : public connection
{
 private:
 public:
	char name[MAXBUF]; 	// server name
	long pingtime;		// last ping response (ms)
	long usercount_i;	// invisible users on server
	long usercount;		// non-invisible users on server
	long opercount;		// opers on server
	int hops_away;	// number of hops away (for quick access)
	long version;		// ircd version
	bool jupiter;		// is a JUPE server (faked to enforce a server ban)

	serverrec();
	serverrec(char* n, long ver, bool jupe);
	~serverrec();
};



typedef map<string, serverrec*> server_list;

#endif

