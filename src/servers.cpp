/*


*/

#include "inspircd_config.h" 
#include "servers.h"
#include "inspircd.h"
#include <stdio.h>
#include <map.h>

serverrec::serverrec()
{
	leaf.clear();
	strcpy(name,"");
	pingtime = 0;
	linktype = LINK_ACTIVE;
	lastping = time(NULL);
	usercount_i = usercount = opercount = version = 0;
	hops_away = 1;
	connected_at = time(NULL);
	jupiter = false;
	fd = 0;
}

 
serverrec::~serverrec()
{
}

serverrec::serverrec(char* n, int link_t,  long ver, bool jupe)
{
	leaf.clear();
	strcpy(name,n);
	linktype = link_t;
	lastping = time(NULL);
	usercount_i = usercount = opercount = 0;
	version = ver;
	hops_away = 1;
	connected_at = time(NULL);
	jupiter = jupe;
	fd = 0;
}

void serverrec::AddLeaf(serverrec *child)
{
	leaf[child->name] = child;
}

void serverrec::DelLeaf(string n)
{
	server_list::iterator i = leaf.find(n);

	if (i != leaf.end())
		leaf.erase(i);
}

