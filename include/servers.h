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
	map<string, serverrec*> leaf; // list of child servers (leaves)
 public:
	char name[MAXBUF]; 	// server name
	int pingtime;		// last ping response (ms)
	int linktype;		// link type, LINK_ACTIVE or LINK_INACTIVE
	time_t lastping;	// time the link was last pinged
	long usercount_i;	// invisible users on server
	long usercount;		// non-invisible users on server
	long opercount;		// opers on server
	time_t connected_at;	// time server was connected into the network
	time_t hops_away;	// number of hops away (for quick access)
	long version;		// ircd version
	bool jupiter;		// is a JUPE server (faked to enforce a server ban)

	serverrec();
	serverrec(char* n, int link_t,  long ver, bool jupe);
	~serverrec();
	void AddLeaf(serverrec *child);
	void DelLeaf(string n);
};



typedef map<string, serverrec*> server_list;

#endif

