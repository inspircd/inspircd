/*

*/

#include "inspircd_config.h" 
#include "channels.h"
#include "users.h"
#include "inspircd.h"
#include <stdio.h>

userrec::userrec()
{
	// the PROPER way to do it, AVOID bzero at *ALL* costs
	strcpy(nick,"");
	ip = 0;
	strcpy(ident,"");
	strcpy(host,"");
	strcpy(dhost,"");
	strcpy(fullname,"");
	strcpy(modes,"");
	strcpy(inbuf,"");
	strcpy(server,"");
	strcpy(awaymsg,"");
	fd = lastping = signon = idle_lastmsg = nping = registered = 0;
	port = bytes_in = bytes_out = cmds_in = cmds_out = 0;
	haspassed = false;
	strcpy(result,"");
	for (int i = 0; i < MAXCHANS; i++)
	{
		chans[i].channel = NULL;
	}
	invites.clear();
}

 
char* userrec::GetFullHost()
{
	sprintf(result,"%s!%s@%s",nick,ident,dhost);
	return result;
}


char* userrec::GetFullRealHost()
{
	sprintf(result,"%s!%s@%s",nick,ident,host);
	return result;
}

bool userrec::IsInvited(char* channel)
{
	for (InvitedList::iterator i = invites.begin(); i != invites.end(); i++)
	{
		if (!strcasecmp(i->channel,channel))
		{
			return true;
		}
	}
}

void userrec::InviteTo(char* channel)
{
	Invited i;
	strcpy(i.channel,channel);
	invites.push_back(i);
}

void userrec::RemoveInvite(char* channel)
{
        for (InvitedList::iterator i = invites.begin(); i != invites.end(); i++)
        {
                if (!strcasecmp(i->channel,channel))
                {
                        invites.erase(i);
			return;
                }
        }
}
