/*


*/

#include "inspircd_config.h" 
#include "servers.h"
#include "inspircd.h"
#include <stdio.h>
#include <map.h>

serverrec::serverrec()
{
	strcpy(name,"");
	pingtime = 0;
	lastping = time(NULL);
	usercount_i = usercount = opercount = version = 0;
	hops_away = 1;
	signon = time(NULL);
	jupiter = false;
	fd = 0;
}

 
serverrec::~serverrec()
{
}

serverrec::serverrec(char* n, long ver, bool jupe)
{
	strcpy(name,n);
	lastping = time(NULL);
	usercount_i = usercount = opercount = 0;
	version = ver;
	hops_away = 1;
	signon = time(NULL);
	jupiter = jupe;
	fd = 0;
}

