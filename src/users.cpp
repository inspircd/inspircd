/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd_config.h" 
#include "channels.h"
#include "users.h"
#include "inspircd.h"
#include <stdio.h>
#include "inspstring.h"

extern std::stringstream config_f;

extern time_t TIME;

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
	strcpy(oper,"");
	fd = lastping = signon = idle_lastmsg = nping = registered = 0;
	flood = port = bytes_in = bytes_out = cmds_in = cmds_out = 0;
	haspassed = false;
	dns_done = false;
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
	snprintf(result,MAXBUF,"%s!%s@%s",nick,ident,dhost);
	return result;
}


char* userrec::GetFullRealHost()
{
	snprintf(result,MAXBUF,"%s!%s@%s",nick,ident,host);
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
	strlcpy(i.channel,channel,CHANMAX);
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

bool userrec::HasPermission(char* command)
{
	char TypeName[MAXBUF],Classes[MAXBUF],ClassName[MAXBUF],CommandList[MAXBUF];
	char* myclass;
	char* mycmd;
	char* savept;
	char* savept2;
	
	// are they even an oper at all?
	if (strchr(this->modes,'o'))
	{
		log(DEBUG,"*** HasPermission: %s is an oper",this->nick);
		for (int j =0; j < ConfValueEnum("type",&config_f); j++)
		{
			ConfValue("type","name",j,TypeName,&config_f);
			if (!strcmp(TypeName,this->oper))
			{
				log(DEBUG,"*** HasPermission: %s is an oper of type '%s'",this->nick,this->oper);
				ConfValue("type","classes",j,Classes,&config_f);
				char* myclass = strtok_r(Classes," ",&savept);
				while (myclass)
				{
					log(DEBUG,"*** HasPermission: checking classtype '%s'",myclass);
					for (int k =0; k < ConfValueEnum("class",&config_f); k++)
					{
						ConfValue("class","name",k,ClassName,&config_f);
						if (!strcmp(ClassName,myclass))
						{
							ConfValue("class","commands",k,CommandList,&config_f);
							log(DEBUG,"*** HasPermission: found class named %s with commands: '%s'",ClassName,CommandList);
							
							
							mycmd = strtok_r(CommandList," ",&savept2);
							while (mycmd)
							{
								if (!strcasecmp(mycmd,command))
								{
									log(DEBUG,"*** Command %s found, returning true",command);
									return true;
								}
								mycmd = strtok_r(NULL," ",&savept2);
							}
						}
					}
					myclass = strtok_r(NULL," ",&savept);
				}
			}
		}
	}
	return false;
}


