/*

$Log$
Revision 1.1  2003/01/26 20:15:03  brain
Added server classes for linking


*/

#include "inspircd_config.h" 
#include "servers.h"
#include "inspircd.h"
#include <stdio.h>

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
}

void serverrec::AddLeaf(serverrec *child)
{
	leaf.push_back(child);
}

void serverrec::DelLeaf(char* n)
{
	for (server_list::iterator i = leaf.begin(); i != leaf.end(); i++)
	{
		if (strcasecmp(n,i->name))
		{
			leaf.erase(i);
			return;
		}
	}
}

