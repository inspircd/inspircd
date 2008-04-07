/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	  the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $ModDesc: Provides a spanning tree server link protocol */
		
#include "inspircd.h"
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "wildcard.h"
#include "xline.h"      
#include "transport.h"  
			
#include "m_spanningtree/timesynctimer.h"
#include "m_spanningtree/resolvers.h"
#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/link.h"
#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/rconnect.h"
#include "m_spanningtree/rsquit.h"

/* $ModDep: m_spanningtree/timesynctimer.h m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h m_spanningtree/rconnect.h m_spanningtree/rsquit.h */

const std::string ModuleSpanningTree::MapOperInfo(TreeServer* Current)
{		       
	time_t secs_up = ServerInstance->Time() - Current->age;
	return (" [Up: " + TimeToStr(secs_up) + " Lag: "+ConvToStr(Current->rtt)+"ms]");
}	       
		
// WARNING: NOT THREAD SAFE - DONT GET ANY SMART IDEAS.
void ModuleSpanningTree::ShowMap(TreeServer* Current, User* user, int depth, char matrix[128][128], float &totusers, float &totservers)
{
	ServerInstance->Logs->Log("map",DEBUG,"ShowMap depth %d totusers %0.2f totservers %0.2f", depth, totusers, totservers);
	if (line < 128)
	{	       
		for (int t = 0; t < depth; t++)
		{
			ServerInstance->Logs->Log("map",DEBUG,"Zero to depth");
			matrix[line][t] = ' ';
		}
       
		// For Aligning, we need to work out exactly how deep this thing is, and produce
		// a 'Spacer' String to compensate.
		char spacer[40];
		memset(spacer,' ',40);
		if ((40 - Current->GetName().length() - depth) > 1) {
			spacer[40 - Current->GetName().length() - depth] = '\0';
		}
		else
		{
			spacer[5] = '\0';
		}       

		float percent;
		char text[128];
		/* Neat and tidy default values, as we're dealing with a matrix not a simple string */
		memset(text, 0, 128);

		if (ServerInstance->Users->clientlist->size() == 0)
		{
			// If there are no users, WHO THE HELL DID THE /MAP?!?!?!
			percent = 0;
		}
		else
		{
			percent = ((float)Current->GetUserCount() / (float)ServerInstance->Users->clientlist->size()) * 100;
		}

		const std::string operdata = IS_OPER(user) ? MapOperInfo(Current) : "";
		snprintf(text, 126, "%s (%s)%s%5d [%5.2f%%]%s", Current->GetName().c_str(), Current->GetID().c_str(), spacer, Current->GetUserCount(), percent, operdata.c_str());
		totusers += Current->GetUserCount();
		totservers++;
		strlcpy(&matrix[line][depth],text,126);
		line++;

		ServerInstance->Logs->Log("map",DEBUG,"Increment line to %d, ChildCount %d", line, Current->ChildCount());

		for (unsigned int q = 0; q < Current->ChildCount(); q++)
		{
			ServerInstance->Logs->Log("map",DEBUG,"Hidden? %d HideULines? %d GetName %s", Current->GetChild(q)->Hidden, Utils->HideULines, Current->GetChild(q)->GetName().c_str());
			if ((Current->GetChild(q)->Hidden) || ((Utils->HideULines) && (ServerInstance->ULine(Current->GetChild(q)->GetName().c_str()))))
			{
				if (*user->oper)
				{
					ShowMap(Current->GetChild(q),user,(Utils->FlatLinks && (!*user->oper)) ? depth : depth+2,matrix,totusers,totservers);
					ServerInstance->Logs->Log("map",DEBUG,"Show to oper");
				}
				ServerInstance->Logs->Log("map",DEBUG,"Fall through");
			}
			else
			{
				ShowMap(Current->GetChild(q),user,(Utils->FlatLinks && (!*user->oper)) ? depth : depth+2,matrix,totusers,totservers);
				ServerInstance->Logs->Log("map",DEBUG,"Show to non oper");
			}
		}
		ServerInstance->Logs->Log("map",DEBUG,"After loop");
	}
}


// Ok, prepare to be confused.
// After much mulling over how to approach this, it struck me that
// the 'usual' way of doing a /MAP isnt the best way. Instead of
// keeping track of a ton of ascii characters, and line by line
// under recursion working out where to place them using multiplications
// and divisons, we instead render the map onto a backplane of characters
// (a character matrix), then draw the branches as a series of "L" shapes
// from the nodes. This is not only friendlier on CPU it uses less stack.
int ModuleSpanningTree::HandleMap(const char* const* parameters, int pcnt, User* user)
{
	if (pcnt > 0)
	{
		/* Remote MAP, the server is within the 1st parameter */
		TreeServer* s = Utils->FindServerMask(parameters[0]);
		bool ret = false;
		if (!s)
		{
			user->WriteServ( "402 %s %s :No such server", user->nick, parameters[0]);
			ret = true;
		}
		else if (s && s != Utils->TreeRoot)
		{
			std::deque<std::string> params;
			params.push_back(parameters[0]);

			params[0] = s->GetName();
			Utils->DoOneToOne(user->uuid, "MAP", params, s->GetName());
			ret = true;
		}

		// Don't return if s == Utils->TreeRoot (us)
		if (ret)
			return 1;
	}

	// This array represents a virtual screen which we will
	// "scratch" draw to, as the console device of an irc
	// client does not provide for a proper terminal.
	float totusers = 0;
	float totservers = 0;
	static char matrix[128][128];

	for (unsigned int t = 0; t < 128; t++)
	{
		matrix[t][0] = '\0';
	}

	line = 0;

	// The only recursive bit is called here.
	ShowMap(Utils->TreeRoot,user,0,matrix,totusers,totservers);

	// Process each line one by one. The algorithm has a limit of
	// 128 servers (which is far more than a spanning tree should have
	// anyway, so we're ok). This limit can be raised simply by making
	// the character matrix deeper, 128 rows taking 10k of memory.
	for (int l = 1; l < line; l++)
	{
		// scan across the line looking for the start of the
		// servername (the recursive part of the algorithm has placed
		// the servers at indented positions depending on what they
		// are related to)
		int first_nonspace = 0;

		while (matrix[l][first_nonspace] == ' ')
		{
			first_nonspace++;
		}

		first_nonspace--;

		// Draw the `- (corner) section: this may be overwritten by
		// another L shape passing along the same vertical pane, becoming
		// a |- (branch) section instead.

		matrix[l][first_nonspace] = '-';
		matrix[l][first_nonspace-1] = '`';
		int l2 = l - 1;

		// Draw upwards until we hit the parent server, causing possibly
		// other corners (`-) to become branches (|-)
		while ((matrix[l2][first_nonspace-1] == ' ') || (matrix[l2][first_nonspace-1] == '`'))
		{
			matrix[l2][first_nonspace-1] = '|';
			l2--;
		}
	}

	float avg_users = totusers / totservers;

	// dump the whole lot to the user.
	if (IS_LOCAL(user))
	{
		ServerInstance->Logs->Log("map",DEBUG,"local");
		for (int t = 0; t < line; t++)
		{
			user->WriteNumeric(6, "%s :%s",user->nick,&matrix[t][0]);
		}
		user->WriteNumeric(270, "%s :%.0f server%s and %.0f user%s, average %.2f users per server",user->nick,totservers,(totservers > 1 ? "s" : ""),totusers,(totusers > 1 ? "s" : ""),avg_users);
		user->WriteNumeric(7, "%s :End of /MAP",user->nick);
	}
	else
	{

		//void SpanningTreeProtocolInterface::PushToClient(User* target, const std::string &rawline)
		//
		ServerInstance->Logs->Log("map", DEBUG, "remote dump lines=%d", line);

		for (int t = 0; t < line; t++)
		{
			ServerInstance->Logs->Log("map",DEBUG,"Dump %d", line);
			ServerInstance->PI->PushToClient(user, std::string("::") + ServerInstance->Config->ServerName + " 006 " + user->nick + " :" + &matrix[t][0]);
		}

		ServerInstance->PI->PushToClient(user, std::string("::") + ServerInstance->Config->ServerName + " 270 " + user->nick + " :" + ConvToStr(totservers) + " server"+(totservers > 1 ? "s" : "") + " and " + ConvToStr(totusers) + " user"+(totusers > 1 ? "s" : "") + ", average " + ConvToStr(avg_users) + " users per server");
		ServerInstance->PI->PushToClient(user, std::string("::") + ServerInstance->Config->ServerName + " 007 " + user->nick + " :End of /MAP");
	}

	return 1;
}

