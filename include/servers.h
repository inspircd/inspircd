/*


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

