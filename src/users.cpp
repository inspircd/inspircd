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
	strcpy(ip,"127.0.0.1");
	timeout = 0;
	strcpy(ident,"");
	strcpy(host,"");
	strcpy(dhost,"");
	strcpy(fullname,"");
	strcpy(modes,"");
	strcpy(inbuf,"");
	strcpy(server,"");
	strcpy(awaymsg,"");
	fd = lastping = signon = idle_lastmsg = nping = registered = 0;
	flood = port = bytes_in = bytes_out = cmds_in = cmds_out = 0;
	haspassed = false;
	strcpy(result,"");
	for (int i = 0; i < MAXCHANS; i++)
	{
		this->chans[i].channel = NULL;
		this->chans[i].uc_modes = 0;
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
		if (i->channel) {
			if (!strcasecmp(i->channel,channel))
			{
				return true;
			}
		}
	}
	return false;
}

void userrec::InviteTo(char* channel)
{
	Invited i;
	strcpy(i.channel,channel);
	invites.push_back(i);
}

void userrec::RemoveInvite(char* channel)
{
	log(DEBUG,"Removing invites");
	if (channel)
	{
		if (invites.size())
		{
 			for (InvitedList::iterator i = invites.begin(); i != invites.end(); i++)
 			{
				if (i->channel)
				{
					if (!strcasecmp(i->channel,channel))
					{
						invites.erase(i);
						return;
		       	         	}
				}
        		}
        	}
        }
}
