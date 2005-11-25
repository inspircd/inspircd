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

using namespace std;

#include "inspircd_config.h" 
#include "channels.h"
#include "connection.h"
#include "users.h"
#include "inspircd.h"
#include <stdio.h>
#include "inspstring.h"
#include "helperfuncs.h"

extern std::stringstream config_f;
extern char ServerName[MAXBUF];

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
	server = (char*)FindServerNamePtr(ServerName);
	strcpy(awaymsg,"");
	strcpy(oper,"");
	reset_due = TIME;
	lines_in = 0;
	fd = lastping = signon = idle_lastmsg = nping = registered = 0;
	flood = port = bytes_in = bytes_out = cmds_in = cmds_out = 0;
	haspassed = false;
	dns_done = false;
	recvq = "";
	sendq = "";
	for (int i = 0; i < MAXCHANS; i++)
	{
		this->chans[i].channel = NULL;
		this->chans[i].uc_modes = 0;
	}
	invites.clear();
}

void userrec::CloseSocket()
{
	shutdown(this->fd,2);
	close(this->fd);
}
 
char* userrec::GetFullHost()
{
	static char result[MAXBUF];
	snprintf(result,MAXBUF,"%s!%s@%s",nick,ident,dhost);
	return result;
}

int userrec::ReadData(void* buffer, size_t size)
{
	if (this->fd > -1)
	{
		log(DEBUG,"userrec::ReadData on fd %d",this->fd);
		return read(this->fd, buffer, size);
	}
	else return 0;
}


char* userrec::GetFullRealHost()
{
	static char fresult[MAXBUF];
	snprintf(fresult,MAXBUF,"%s!%s@%s",nick,ident,host);
	return fresult;
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

InvitedList* userrec::GetInviteList()
{
	return &invites;
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


bool userrec::AddBuffer(std::string a)
{
        std::string b = "";
        for (unsigned int i = 0; i < a.length(); i++)
                if ((a[i] != '\r') && (a[i] != '\0') && (a[i] != 7))
                        b = b + a[i];
        std::stringstream stream(recvq);
        stream << b;
        recvq = stream.str();
	unsigned int i = 0;
	// count the size of the first line in the buffer.
	while (i < recvq.length())
	{
		if (recvq[i++] == '\n')
			break;
	}
	if (recvq.length() > (unsigned)this->recvqmax)
	{
		this->SetWriteError("RecvQ exceeded");
		WriteOpers("*** User %s RecvQ of %d exceeds connect class maximum of %d",this->nick,recvq.length(),this->recvqmax);
	}
	// return false if we've had more than 600 characters WITHOUT
	// a carriage return (this is BAD, drop the socket)
	return (i < 600);
}

bool userrec::BufferIsReady()
{
        for (unsigned int i = 0; i < recvq.length(); i++)
		if (recvq[i] == '\n')
			return true;
        return false;
}

void userrec::ClearBuffer()
{
        recvq = "";
}

std::string userrec::GetBuffer()
{
	if (recvq == "")
		return "";
        char* line = (char*)recvq.c_str();
        std::string ret = "";
        while ((*line != '\n') && (strlen(line)))
        {
                ret = ret + *line;
                line++;
        }
        if ((*line == '\n') || (*line == '\r'))
                line++;
        recvq = line;
        return ret;
}

void userrec::AddWriteBuf(std::string data)
{
	if (this->GetWriteError() != "")
		return;
	if (sendq.length() + data.length() > (unsigned)this->sendqmax)
	{
		WriteOpers("*** User %s SendQ of %d exceeds connect class maximum of %d",this->nick,sendq.length() + data.length(),this->sendqmax);
		this->SetWriteError("SendQ exceeded");
		return;
	}
        std::stringstream stream;
        stream << sendq << data;
        sendq = stream.str();
}

// send AS MUCH OF THE USERS SENDQ as we are able to (might not be all of it)
void userrec::FlushWriteBuf()
{
	if (sendq.length())
	{
		char* tb = (char*)this->sendq.c_str();
		int n_sent = write(this->fd,tb,this->sendq.length());
		if (n_sent == -1)
		{
			this->SetWriteError(strerror(errno));
		}
		else
		{
			// advance the queue
			tb += n_sent;
			this->sendq = tb;
			// update the user's stats counters
			this->bytes_out += n_sent;
			this->cmds_out++;
		}
	}
}

void userrec::SetWriteError(std::string error)
{
	log(DEBUG,"Setting error string for %s to '%s'",this->nick,error.c_str());
	// don't try to set the error twice, its already set take the first string.
	if (this->WriteError == "")
		this->WriteError = error;
}

std::string userrec::GetWriteError()
{
	return this->WriteError;
}
