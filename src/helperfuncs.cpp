/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include "inspircd_config.h"
#include "inspircd.h"
#include "inspircd_io.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <time.h>
#include <string>
#ifdef GCC3
#include <ext/hash_map>
#else
#include <hash_map>
#endif
#include <sstream>
#include <vector>
#include <deque>
#include <stdarg.h>
#include "connection.h"
#include "users.h"
#include "ctables.h"
#include "globals.h"
#include "modules.h"
#include "dynamic.h"
#include "wildcard.h"
#include "message.h"
#include "mode.h"
#include "xline.h"
#include "commands.h"
#include "inspstring.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "typedefs.h"

extern int MODCOUNT;
extern std::vector<Module*> modules;
extern ServerConfig *Config;
extern InspIRCd* ServerInstance;
extern time_t TIME;
extern char lowermap[255];
extern userrec* fd_ref_table[MAX_DESCRIPTORS];
static char already_sent[MAX_DESCRIPTORS];
extern std::vector<userrec*> all_opers;
extern user_hash clientlist;
extern chan_hash chanlist;

extern std::vector<userrec*> local_users;

static char TIMESTR[26];
static time_t LAST = 0;

/** log()
 *  Write a line of text `text' to the logfile (and stdout, if in nofork) if the level `level'
 *  is greater than the configured loglevel.
 */
void log(int level, char *text, ...)
{
	va_list argsPtr;
	char textbuffer[MAXBUF];

	if (level < Config->LogLevel)
		return;

	if (TIME != LAST)
	{
		struct tm *timeinfo = localtime(&TIME);

		strlcpy(TIMESTR,asctime(timeinfo),26);
		TIMESTR[24] = ':';
		LAST = TIME;
	}

	if (Config->log_file)
	{
		va_start(argsPtr, text);
		vsnprintf(textbuffer, MAXBUF, text, argsPtr);
		va_end(argsPtr);

		if (Config->log_file)
			fprintf(Config->log_file,"%s %s\n",TIMESTR,textbuffer);

		if (Config->nofork)
		{
			printf("%s %s\n", TIMESTR, textbuffer);
		}
	}
}

/** readfile()
 *  Read the contents of a file located by `fname' into a file_cache pointed at by `F'.
 *
 *  XXX - we may want to consider returning a file_cache or pointer to one, less confusing.
 */
void readfile(file_cache &F, const char* fname)
{
	FILE* file;
	char linebuf[MAXBUF];

	log(DEBUG,"readfile: loading %s",fname);
	F.clear();
	file =  fopen(fname,"r");

	if (file)
	{
		while (!feof(file))
		{
			fgets(linebuf,sizeof(linebuf),file);
			linebuf[strlen(linebuf)-1]='\0';

			if (!*linebuf)
			{
				strcpy(linebuf,"  ");
			}

			if (!feof(file))
			{
				F.push_back(linebuf);
			}
		}

		fclose(file);
	}
	else
	{
		log(DEBUG,"readfile: failed to load file: %s",fname);
	}

	log(DEBUG,"readfile: loaded %s, %lu lines",fname,(unsigned long)F.size());
}

/** Write_NoFormat()
 *  Writes a given string in `text' to the socket on fd `sock' - only if the socket
 *  is a valid entry in the local FD table.
 */
void Write_NoFormat(int sock, const char *text)
{
	char tb[MAXBUF];
	int bytes;

	if ((sock < 0) || (!text) || (sock > MAX_DESCRIPTORS))
		return;

	bytes = snprintf(tb,MAXBUF,"%s\r\n",text);
	chop(tb);

	if (fd_ref_table[sock])
	{
		if (Config->GetIOHook(fd_ref_table[sock]->port))
		{
			try
			{
				Config->GetIOHook(fd_ref_table[sock]->port)->OnRawSocketWrite(sock,tb,bytes);
			}
			catch (ModuleException& modexcept)
			{
				log(DEBUG,"Module exception caught: %s",modexcept.GetReason());
			}
		}
		else
		{
			fd_ref_table[sock]->AddWriteBuf(tb);
		}
		ServerInstance->stats->statsSent += bytes;
	}
	else
		log(DEFAULT,"ERROR! attempted write to a user with no fd_ref_table entry!!!");
}

/** Write()
 *  Same as Write_NoFormat(), but formatted printf() style first.
 */
void Write(int sock, char *text, ...)
{
	va_list argsPtr;
	char textbuffer[MAXBUF];
	char tb[MAXBUF];
	int bytes;

	if ((sock < 0) || (sock > MAX_DESCRIPTORS))
		return;

	if (!text)
	{
		log(DEFAULT,"*** BUG *** Write was given an invalid parameter");
		return;
	}

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);
	bytes = snprintf(tb,MAXBUF,"%s\r\n",textbuffer);
	chop(tb);

	if (fd_ref_table[sock])
	{
		if (Config->GetIOHook(fd_ref_table[sock]->port))
		{
			try
			{
				Config->GetIOHook(fd_ref_table[sock]->port)->OnRawSocketWrite(sock,tb,bytes);
			}
			catch (ModuleException& modexcept)
			{
				log(DEBUG,"Module exception caught: %s",modexcept.GetReason());
			}						
		}
		else
		{
			fd_ref_table[sock]->AddWriteBuf(tb);
		}
		ServerInstance->stats->statsSent += bytes;
	}
	else
		log(DEFAULT,"ERROR! attempted write to a user with no fd_ref_table entry!!!");
}

/** WriteServ_NoFormat()
 *  Same as Write_NoFormat(), except prefixes `text' with `:server.name '.
 */
void WriteServ_NoFormat(int sock, const char* text)
{
	char tb[MAXBUF];
	int bytes;

	if ((sock < 0) || (!text) || (sock > MAX_DESCRIPTORS))
		return;

	bytes = snprintf(tb,MAXBUF,":%s %s\r\n",Config->ServerName,text);
	chop(tb);

	if (fd_ref_table[sock])
	{
		if (Config->GetIOHook(fd_ref_table[sock]->port))
		{
			try
			{
				Config->GetIOHook(fd_ref_table[sock]->port)->OnRawSocketWrite(sock,tb,bytes);
			}
			catch (ModuleException& modexcept)
			{
				log(DEBUG,"Module exception caught: %s",modexcept.GetReason());
			}
		}
		else
		{
			fd_ref_table[sock]->AddWriteBuf(tb);
		}
		ServerInstance->stats->statsSent += bytes;
	}
	else
		log(DEFAULT,"ERROR! attempted write to a user with no fd_ref_table entry!!!");
}

/** WriteServ()
 *  Same as Write(), except `text' is prefixed with `:server.name '.
 */
void WriteServ(int sock, char* text, ...)
{
	va_list argsPtr;
	char textbuffer[MAXBUF];
	char tb[MAXBUF];
	int bytes;

	if ((sock < 0) || (sock > MAX_DESCRIPTORS))
		return;

	if (!text)
	{
		log(DEFAULT,"*** BUG *** WriteServ was given an invalid parameter");
		return;
	}

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);
	bytes = snprintf(tb,MAXBUF,":%s %s\r\n",Config->ServerName,textbuffer);
	chop(tb);

	if (fd_ref_table[sock])
	{
		if (Config->GetIOHook(fd_ref_table[sock]->port))
		{
			try
			{
				Config->GetIOHook(fd_ref_table[sock]->port)->OnRawSocketWrite(sock,tb,bytes);
			}
			catch (ModuleException& modexcept)
			{
				log(DEBUG,"Module exception caught: %s",modexcept.GetReason());
			}
		}
		else
		{
			fd_ref_table[sock]->AddWriteBuf(tb);
		}

		ServerInstance->stats->statsSent += bytes;
	}
	else
		log(DEFAULT,"ERROR! attempted write to a user with no fd_ref_table entry!!!");
}

/** WriteFrom_NoFormat()
 * Write `text' to a socket with fd `sock' prefixed with `:n!u@h' - taken from
 * the nick, user, and host of `user'.
 */
void WriteFrom_NoFormat(int sock, userrec *user, const char* text)
{
	char tb[MAXBUF];
	int bytes;

	if ((sock < 0) || (!text) || (!user) || (sock > MAX_DESCRIPTORS))
		return;

	bytes = snprintf(tb,MAXBUF,":%s %s\r\n",user->GetFullHost(),text);
	chop(tb);

	if (fd_ref_table[sock])
	{
		if (Config->GetIOHook(fd_ref_table[sock]->port))
		{
			try
			{
				Config->GetIOHook(fd_ref_table[sock]->port)->OnRawSocketWrite(sock,tb,bytes);
			}
			catch (ModuleException& modexcept)
			{
				log(DEBUG,"Module exception caught: %s",modexcept.GetReason());
			}
		}
		else
		{
			fd_ref_table[sock]->AddWriteBuf(tb);
		}
		ServerInstance->stats->statsSent += bytes;
	}
	else
		log(DEFAULT,"ERROR! attempted write to a user with no fd_ref_table entry!!!");
}

/* write text from an originating user to originating user */

void WriteFrom(int sock, userrec *user,char* text, ...)
{
	va_list argsPtr;
	char textbuffer[MAXBUF];
	char tb[MAXBUF];
	int bytes;

	if ((sock < 0) || (sock > MAX_DESCRIPTORS))
		return;

	if ((!text) || (!user))
	{
		log(DEFAULT,"*** BUG *** WriteFrom was given an invalid parameter");
		return;
	}

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);
	bytes = snprintf(tb,MAXBUF,":%s %s\r\n",user->GetFullHost(),textbuffer);
	chop(tb);

	if (fd_ref_table[sock])
	{
		if (Config->GetIOHook(fd_ref_table[sock]->port))
		{
			try
			{
				Config->GetIOHook(fd_ref_table[sock]->port)->OnRawSocketWrite(sock,tb,bytes);
			}
			catch (ModuleException& modexcept)
			{
				log(DEBUG,"Module exception caught: %s",modexcept.GetReason());
			}
		}
		else
		{
			fd_ref_table[sock]->AddWriteBuf(tb);
		}

		ServerInstance->stats->statsSent += bytes;
	}
	else
		log(DEFAULT,"ERROR! attempted write to a user with no fd_ref_table entry!!!");
}

/* write text to an destination user from a source user (e.g. user privmsg) */

void WriteTo(userrec *source, userrec *dest,char *data, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;

	if ((!dest) || (!data))
	{
		log(DEFAULT,"*** BUG *** WriteTo was given an invalid parameter");
		return;
	}

	if (!IS_LOCAL(dest))
		return;

	va_start(argsPtr, data);
	vsnprintf(textbuffer, MAXBUF, data, argsPtr);
	va_end(argsPtr);
	chop(textbuffer);

	// if no source given send it from the server.
	if (!source)
	{
		WriteServ_NoFormat(dest->fd,textbuffer);
	}
	else
	{
		WriteFrom_NoFormat(dest->fd,source,textbuffer);
	}
}

void WriteTo_NoFormat(userrec *source, userrec *dest, const char *data)
{
	if ((!dest) || (!data))
		return;

	if (!source)
	{
		WriteServ_NoFormat(dest->fd,data);
	}
	else
	{
		WriteFrom_NoFormat(dest->fd,source,data);
	}
}

/* write formatted text from a source user to all users on a channel
 * including the sender (NOT for privmsg, notice etc!) */

void WriteChannel(chanrec* Ptr, userrec* user, char* text, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;
	CUList *ulist;

	if ((!Ptr) || (!user) || (!text))
	{
		log(DEFAULT,"*** BUG *** WriteChannel was given an invalid parameter");
		return;
	}

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	ulist = Ptr->GetUsers();

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		if (i->second->fd != FD_MAGIC_NUMBER)
			WriteTo_NoFormat(user,i->second,textbuffer);
	}
}

void WriteChannel_NoFormat(chanrec* Ptr, userrec* user, const char* text)
{
	CUList *ulist;

	if ((!Ptr) || (!user) || (!text))
	{
		log(DEFAULT,"*** BUG *** WriteChannel was given an invalid parameter");
		return;
	}

	ulist = Ptr->GetUsers();

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		if (i->second->fd != FD_MAGIC_NUMBER)
			WriteTo_NoFormat(user,i->second,text);
	}
}


/* write formatted text from a source user to all users on a channel
 * including the sender (NOT for privmsg, notice etc!) doesnt send to
 * users on remote servers */

void WriteChannelLocal(chanrec* Ptr, userrec* user, char* text, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;
	CUList *ulist;

	if ((!Ptr) || (!text))
	{
		log(DEFAULT,"*** BUG *** WriteChannel was given an invalid parameter");
		return;
	}

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	ulist = Ptr->GetUsers();

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		if ((i->second->fd != FD_MAGIC_NUMBER) && (i->second != user))
		{
			if (!user)
			{
				WriteServ_NoFormat(i->second->fd,textbuffer);
			}
			else
			{
				WriteTo_NoFormat(user,i->second,textbuffer);
			}
		}
	}
}

void WriteChannelLocal_NoFormat(chanrec* Ptr, userrec* user, const char* text)
{
	CUList *ulist;

	if ((!Ptr) || (!text))
	{
		log(DEFAULT,"*** BUG *** WriteChannel was given an invalid parameter");
		return;
	}

	ulist = Ptr->GetUsers();

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		if ((i->second->fd != FD_MAGIC_NUMBER) && (i->second != user))
		{
			if (!user)
			{
				WriteServ_NoFormat(i->second->fd,text);
			}
			else
			{
				WriteTo_NoFormat(user,i->second,text);
			}
		}
	}
}



void WriteChannelWithServ(char* ServName, chanrec* Ptr, char* text, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;
	CUList *ulist;

	if ((!Ptr) || (!text))
	{
		log(DEFAULT,"*** BUG *** WriteChannelWithServ was given an invalid parameter");
		return;
	}

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	ulist = Ptr->GetUsers();

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		if (IS_LOCAL(i->second))
			WriteServ_NoFormat(i->second->fd,textbuffer);
	}
}

void WriteChannelWithServ_NoFormat(char* ServName, chanrec* Ptr, const char* text)
{
	CUList *ulist;

	if ((!Ptr) || (!text))
	{
		log(DEFAULT,"*** BUG *** WriteChannelWithServ was given an invalid parameter");
		return;
	}

	ulist = Ptr->GetUsers();

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		if (IS_LOCAL(i->second))
			WriteServ_NoFormat(i->second->fd,text);
	}
}



/* write formatted text from a source user to all users on a channel except
 * for the sender (for privmsg etc) */

void ChanExceptSender(chanrec* Ptr, userrec* user, char status, char* text, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;
	CUList *ulist;

	if ((!Ptr) || (!user) || (!text))
	{
		log(DEFAULT,"*** BUG *** ChanExceptSender was given an invalid parameter");
		return;
	}

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	switch (status)
	{
		case '@':
			ulist = Ptr->GetOppedUsers();
			break;
		case '%':
			ulist = Ptr->GetHalfoppedUsers();
			break;
		case '+':
			ulist = Ptr->GetVoicedUsers();
			break;
		default:
			ulist = Ptr->GetUsers();
			break;
	}

	log(DEBUG,"%d users to write to",ulist->size());

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		if ((IS_LOCAL(i->second)) && (user != i->second))
			WriteFrom_NoFormat(i->second->fd,user,textbuffer);
	}
}

void ChanExceptSender_NoFormat(chanrec* Ptr, userrec* user, char status, const char* text)
{
	CUList *ulist;

	if ((!Ptr) || (!user) || (!text))
	{
		log(DEFAULT,"*** BUG *** ChanExceptSender was given an invalid parameter");
		return;
	}

	switch (status)
	{
		case '@':
			ulist = Ptr->GetOppedUsers();
			break;  
		case '%':
			ulist = Ptr->GetHalfoppedUsers();
			break;
		case '+':
			ulist = Ptr->GetVoicedUsers();
			break;
		default:
			ulist = Ptr->GetUsers();
			break;
	}

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		if ((IS_LOCAL(i->second)) && (user != i->second))
			WriteFrom_NoFormat(i->second->fd,user,text);
	}
}

std::string GetServerDescription(char* servername)
{
	std::string description = "";

	FOREACH_MOD(I_OnGetServerDescription,OnGetServerDescription(servername,description));

	if (description != "")
	{
		return description;
	}
	else
	{
		// not a remote server that can be found, it must be me.
		return Config->ServerDesc;
	}
}

/* write a formatted string to all users who share at least one common
 * channel, including the source user e.g. for use in NICK */

void WriteCommon(userrec *u, char* text, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;
	bool sent_to_at_least_one = false;

	if (!u)
	{
		log(DEFAULT,"*** BUG *** WriteCommon was given an invalid parameter");
		return;
	}

	if (u->registered != 7)
	{
		log(DEFAULT,"*** BUG *** WriteCommon on an unregistered user");
		return;
	}

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	// FIX: Stops a message going to the same person more than once
	memset(&already_sent,0,MAX_DESCRIPTORS);

	for (std::vector<ucrec*>::const_iterator v = u->chans.begin(); v != u->chans.end(); v++)
	{
		if (((ucrec*)(*v))->channel)
		{
			CUList *ulist= ((ucrec*)(*v))->channel->GetUsers();

			for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
			{
				if ((i->second->fd > -1) && (!already_sent[i->second->fd]))
				{
					already_sent[i->second->fd] = 1;
					WriteFrom_NoFormat(i->second->fd,u,textbuffer);
					sent_to_at_least_one = true;
				}
			}
		}
	}

	/*
	 * if the user was not in any channels, no users will receive the text. Make sure the user
	 * receives their OWN message for WriteCommon
	 */
	if (!sent_to_at_least_one)
	{
		WriteFrom_NoFormat(u->fd,u,textbuffer);
	}
}

void WriteCommon_NoFormat(userrec *u, const char* text)
{
	bool sent_to_at_least_one = false;

	if (!u)
	{
		log(DEFAULT,"*** BUG *** WriteCommon was given an invalid parameter");
		return;
	}

	if (u->registered != 7)
	{
		log(DEFAULT,"*** BUG *** WriteCommon on an unregistered user");
		return;
	}

	// FIX: Stops a message going to the same person more than once
	memset(&already_sent,0,MAX_DESCRIPTORS);

	for (std::vector<ucrec*>::const_iterator v = u->chans.begin(); v != u->chans.end(); v++)
	{
		if (((ucrec*)(*v))->channel)
		{
			CUList *ulist= ((ucrec*)(*v))->channel->GetUsers();

			for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
			{
				if ((i->second->fd > -1) && (!already_sent[i->second->fd]))
				{
					already_sent[i->second->fd] = 1;
					WriteFrom_NoFormat(i->second->fd,u,text);
					sent_to_at_least_one = true;
				}
			}
		}
	}

	/*
	 * if the user was not in any channels, no users will receive the text. Make sure the user
	 * receives their OWN message for WriteCommon
	 */
	if (!sent_to_at_least_one)
	{
		WriteFrom_NoFormat(u->fd,u,text);
	}
}


/* write a formatted string to all users who share at least one common
 * channel, NOT including the source user e.g. for use in QUIT
 */

void WriteCommonExcept(userrec *u, char* text, ...)
{
	char textbuffer[MAXBUF];
	char oper_quit[MAXBUF];
	bool quit_munge = false;
	va_list argsPtr;
	int total;

	if (!u)
	{
		log(DEFAULT,"*** BUG *** WriteCommon was given an invalid parameter");
		return;
	}

	if (u->registered != 7)
	{
		log(DEFAULT,"*** BUG *** WriteCommon on an unregistered user");
		return;
	}

	va_start(argsPtr, text);
	total = vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	if ((Config->HideSplits) && (total > 6))
	{
		/* Yeah yeah, this is ugly. But its fast, live with it. */
		char* check = textbuffer;

		if ((*check++ == 'Q') && (*check++ == 'U') && (*check++ == 'I') && (*check++ == 'T') && (*check++ == ' ') && (*check++ == ':'))
		{
			std::stringstream split(check);
			std::string server_one;
			std::string server_two;

			split >> server_one;
			split >> server_two;

			if ((FindServerName(server_one)) && (FindServerName(server_two)))
			{
				strlcpy(oper_quit,textbuffer,MAXQUIT);
				strlcpy(check,"*.net *.split",MAXQUIT);
				quit_munge = true;
			}
		}
	}

	if ((Config->HideBans) && (total > 13) && (!quit_munge))
	{
		char* check = textbuffer;

		/* XXX - as above */
		if ((*check++ == 'Q') && (*check++ == 'U') && (*check++ == 'I') && (*check++ == 'T') && (*check++ == ' ') && (*check++ == ':'))
		{
			check++;

			if ((*check++ == '-') && (*check++ == 'L') && (*check++ == 'i') && (*check++ == 'n') && (*check++ == 'e') && (*check++ == 'd') && (*check++ == ':'))
			{
				strlcpy(oper_quit,textbuffer,MAXQUIT);
				*(--check) = 0;		// We don't need to strlcpy, we just chop it from the :
				quit_munge = true;
			}
		}
	}

	memset(&already_sent,0,MAX_DESCRIPTORS);

	for (std::vector<ucrec*>::const_iterator v = u->chans.begin(); v != u->chans.end(); v++)
	{
		if (((ucrec*)(*v))->channel)
		{
			CUList *ulist= ((ucrec*)(*v))->channel->GetUsers();

			for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
			{
				if (u != i->second)
				{
					if ((i->second->fd > -1) && (!already_sent[i->second->fd]))
					{
						already_sent[i->second->fd] = 1;

						if (quit_munge)
						{
							WriteFrom_NoFormat(i->second->fd,u,*i->second->oper ? oper_quit : textbuffer);
						}
						else
							WriteFrom_NoFormat(i->second->fd,u,textbuffer);
					}
				}
			}
		}
	}
}

void WriteCommonExcept_NoFormat(userrec *u, const char* text)
{
	if (!u)
	{
		log(DEFAULT,"*** BUG *** WriteCommon was given an invalid parameter");
		return;
	}
 
	if (u->registered != 7)
	{
		log(DEFAULT,"*** BUG *** WriteCommon on an unregistered user");
		return;
	}

	memset(&already_sent,0,MAX_DESCRIPTORS);

	for (std::vector<ucrec*>::const_iterator v = u->chans.begin(); v != u->chans.end(); v++)
	{
		if (((ucrec*)(*v))->channel)
		{
			CUList *ulist= ((ucrec*)(*v))->channel->GetUsers();

			for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
			{
				if (u != i->second)
				{
					if ((i->second->fd > -1) && (!already_sent[i->second->fd]))
					{
						already_sent[i->second->fd] = 1;
						WriteFrom_NoFormat(i->second->fd,u,text);
					}
				}
			}
		}
	}
}


/* XXX - We don't use WriteMode for this because WriteMode is very slow and
 * this isnt. Basically WriteMode has to iterate ALL the users 'n' times for
 * the number of modes provided, e.g. if you send WriteMode 'og' to write to
 * opers with globops, and you have 2000 users, thats 4000 iterations. WriteOpers
 * uses the oper list, which means if you have 2000 users but only 5 opers,
 * it iterates 5 times.
 */
void WriteOpers(char* text, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;

	if (!text)
	{
		log(DEFAULT,"*** BUG *** WriteOpers was given an invalid parameter");
		return;
	}

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	for (std::vector<userrec*>::iterator i = all_opers.begin(); i != all_opers.end(); i++)
	{
		userrec* a = *i;

		if (IS_LOCAL(a))
		{
			if (a->modebits & UM_SERVERNOTICE)
			{
				// send server notices to all with +s
				WriteServ(a->fd,"NOTICE %s :%s",a->nick,textbuffer);
			}
		}
	}
}

void ServerNoticeAll(char* text, ...)
{
	if (!text)
		return;

	char textbuffer[MAXBUF];
	va_list argsPtr;
	va_start (argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	for (std::vector<userrec*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
	{
		userrec* t = (userrec*)(*i);
		WriteServ(t->fd,"NOTICE $%s :%s",Config->ServerName,textbuffer);
	}
}

void ServerPrivmsgAll(char* text, ...)
{
	if (!text)
		return;

	char textbuffer[MAXBUF];
	va_list argsPtr;
	va_start (argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	for (std::vector<userrec*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
	{
		userrec* t = (userrec*)(*i);
		WriteServ(t->fd,"PRIVMSG $%s :%s",Config->ServerName,textbuffer);
	}
}

void WriteMode(const char* modes, int flags, const char* text, ...)
{
	char textbuffer[MAXBUF];
	int modelen;
	va_list argsPtr;

	if ((!text) || (!modes) || (!flags))
	{
		log(DEFAULT,"*** BUG *** WriteMode was given an invalid parameter");
		return;
	}

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);
	modelen = strlen(modes);

	for (std::vector<userrec*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
	{
		userrec* t = (userrec*)(*i);
		bool send_to_user = false;

		if (flags == WM_AND)
		{
			send_to_user = true;

			for (int n = 0; n < modelen; n++)
			{
				if (!hasumode(t,modes[n]))
				{
					send_to_user = false;
					break;
				}
			}
		}
		else if (flags == WM_OR)
		{
			send_to_user = false;

			for (int n = 0; n < modelen; n++)
			{
				if (hasumode(t,modes[n]))
				{
					send_to_user = true;
					break;
				}
			}
		}

		if (send_to_user)
		{
			WriteServ(t->fd,"NOTICE %s :%s",t->nick,textbuffer);
		}
	}
}

void NoticeAll(userrec *source, bool local_only, char* text, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;

	if ((!text) || (!source))
	{
		log(DEFAULT,"*** BUG *** NoticeAll was given an invalid parameter");
		return;
	}

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	for (std::vector<userrec*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
	{
		userrec* t = (userrec*)(*i);
		WriteFrom(t->fd,source,"NOTICE $* :%s",textbuffer);
	}
}


void WriteWallOps(userrec *source, bool local_only, char* text, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;

	if ((!text) || (!source))
	{
		log(DEFAULT,"*** BUG *** WriteOpers was given an invalid parameter");
		return;
	}

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	for (std::vector<userrec*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
	{
		userrec* t = (userrec*)(*i);

		if ((IS_LOCAL(t)) && (t->modebits & UM_WALLOPS))
		{
			WriteTo(source,t,"WALLOPS :%s",textbuffer);
		}
	}
}

/* convert a string to lowercase. Note following special circumstances
 * taken from RFC 1459. Many "official" server branches still hold to this
 * rule so i will too;
 *
 *  Because of IRC's scandanavian origin, the characters {}| are
 *  considered to be the lower case equivalents of the characters []\,
 *  respectively. This is a critical issue when determining the
 *  equivalence of two nicknames.
 */
void strlower(char *n)
{
	if (n)
	{
		for (char* t = n; *t; t++)
			*t = lowermap[(unsigned)*t];
	}
}

/* Find a user record by nickname and return a pointer to it */

userrec* Find(std::string nick)
{
	user_hash::iterator iter = clientlist.find(nick);

	if (iter == clientlist.end())
		/* Couldn't find it */
		return NULL;

	return iter->second;
}

userrec* Find(const char* nick)
{
	user_hash::iterator iter;

	if (!nick)
		return NULL;

	iter = clientlist.find(nick);
	
	if (iter == clientlist.end())
		return NULL;

	return iter->second;
}

/* find a channel record by channel name and return a pointer to it */

chanrec* FindChan(const char* chan)
{
	chan_hash::iterator iter;

	if (!chan)
	{
		log(DEFAULT,"*** BUG *** Findchan was given an invalid parameter");
		return NULL;
	}

	iter = chanlist.find(chan);

	if (iter == chanlist.end())
		/* Couldn't find it */
		return NULL;

	return iter->second;
}


long GetMaxBans(char* name)
{
	std::string x;
	for (std::map<std::string,int>::iterator n = Config->maxbans.begin(); n != Config->maxbans.end(); n++)
	{
		x = n->first;
		if (match(name,x.c_str()))
		{
			return n->second;
		}
	}
	return 64;
}

void purge_empty_chans(userrec* u)
{
	std::vector<chanrec*> to_delete;

	// firstly decrement the count on each channel
	for (std::vector<ucrec*>::iterator f = u->chans.begin(); f != u->chans.end(); f++)
	{
		if (((ucrec*)(*f))->channel)
		{
			if (((ucrec*)(*f))->channel->DelUser(u) == 0)
			{
				/* No users left in here, mark it for deletion */
				to_delete.push_back(((ucrec*)(*f))->channel);
				((ucrec*)(*f))->channel = NULL;
			}
		}
	}

	log(DEBUG,"purge_empty_chans: %d channels to delete",to_delete.size());

	for (std::vector<chanrec*>::iterator n = to_delete.begin(); n != to_delete.end(); n++)
	{
		chanrec* thischan = (chanrec*)*n;
		chan_hash::iterator i2 = chanlist.find(thischan->name);
		if (i2 != chanlist.end())
		{
			FOREACH_MOD(I_OnChannelDelete,OnChannelDelete(i2->second));
			delete i2->second;
			chanlist.erase(i2);
		}
	}

	if (*u->oper)
		DeleteOper(u);
}


char* chanmodes(chanrec *chan, bool showkey)
{
	static char scratch[MAXBUF];
	static char sparam[MAXBUF];
	char* offset = scratch;

	if (!chan)
	{
		log(DEFAULT,"*** BUG *** chanmodes was given an invalid parameter");
		*scratch = '\0';
		return scratch;
	}

	*scratch = '\0';
	*sparam = '\0';

	if (chan->binarymodes & CM_NOEXTERNAL)
		*offset++ = 'n';
	if (chan->binarymodes & CM_TOPICLOCK)
		*offset++ = 't';
	if (*chan->key)
		*offset++ = 'k';
	if (chan->limit)
		*offset++ = 'l';
	if (chan->binarymodes & CM_INVITEONLY)
		*offset++ = 'i';
	if (chan->binarymodes & CM_MODERATED)
		*offset++ = 'm';
	if (chan->binarymodes & CM_SECRET)
		*offset++ = 's';
	if (chan->binarymodes & CM_PRIVATE)
		*offset++ = 'p';

	if (*chan->key)
	{
		snprintf(sparam,MAXBUF," %s",showkey ? chan->key : "<key>");
	}

	if (chan->limit)
	{
		char foo[24];
		sprintf(foo," %lu",(unsigned long)chan->limit);
		strlcat(sparam,foo,MAXBUF);
	}

	/* This was still iterating up to 190, chanrec::custom_modes is only 64 elements -- Om */
	for(int n = 0; n < 64; n++)
	{
		if(chan->custom_modes[n])
		{
			*offset++ = n+65;
			std::string extparam = chan->GetModeParameter(n+65);

			if (extparam != "")
			{
				charlcat(sparam,' ',MAXBUF);
				strlcat(sparam,extparam.c_str(),MAXBUF);
			}
		}
	}

	/* Null terminate scratch */
	*offset = '\0';
	strlcat(scratch,sparam,MAXMODES);
	return scratch;
}


/* compile a userlist of a channel into a string, each nick seperated by
 * spaces and op, voice etc status shown as @ and + */

void userlist(userrec *user,chanrec *c)
{
	if ((!c) || (!user))
	{
		log(DEFAULT,"*** BUG *** userlist was given an invalid parameter");
		return;
	}

	char list[MAXBUF];
	size_t dlen = snprintf(list,MAXBUF,"353 %s = %s :", user->nick, c->name);
	int numusers = 0;
	char* ptr = list + dlen - 1;

	CUList *ulist= c->GetUsers();

	/* Improvement by Brain - this doesnt change in value, so why was it inside
	 * the loop?
	 */
	bool has_user = c->HasUser(user);

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		if ((!has_user) && (i->second->modebits & UM_INVISIBLE))
		{
			/*
			 * user is +i, and source not on the channel, does not show
			 * nick in NAMES list
			 */
			continue;
		}

		const char* n = cmode(i->second,c);
		if (*n)
			*ptr++ = *n;
		for (char* t = i->second->nick; *t; t++)
			*ptr++ = *t;
		*ptr++ = ' ';
		numusers++;

		if ((ptr - list) > (480-NICKMAX))
		{
			/* list overflowed into multiple numerics */
			*--ptr = 0;
			WriteServ_NoFormat(user->fd,list);
			dlen = snprintf(list,MAXBUF,"353 %s = %s :", user->nick, c->name);
			ptr = list + dlen - 1;
			numusers = 0;
		}
	}
	*--ptr = 0;

	/* if whats left in the list isnt empty, send it */
	if (numusers)
	{
		WriteServ_NoFormat(user->fd,list);
	}
}

/*
 * return a count of the users on a specific channel accounting for
 * invisible users who won't increase the count. e.g. for /LIST
 */
int usercount_i(chanrec *c)
{
	int count = 0;

	if (!c)
		return 0;

	CUList *ulist= c->GetUsers();
	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		if (!(i->second->modebits & UM_INVISIBLE))
			count++;
	}

	return count;
}

int usercount(chanrec *c)
{
	return (c ? c->GetUserCounter() : 0);
}


/* looks up a users password for their connection class (<ALLOW>/<DENY> tags) */
ConnectClass GetClass(userrec *user)
{
	for (ClassVector::iterator i = Config->Classes.begin(); i != Config->Classes.end(); i++)
	{
		if (match(user->host,i->host.c_str()))
		{
			return *i;
		}
	}

	return *(Config->Classes.begin());
}

/*
 * sends out an error notice to all connected clients (not to be used
 * lightly!)
 */
void send_error(char *s)
{
	log(DEBUG,"send_error: %s",s);

	for (std::vector<userrec*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
	{
		userrec* t = (userrec*)(*i);
		if (t->registered == 7)
		{
		   	WriteServ(t->fd,"NOTICE %s :%s",t->nick,s);
	   	}
		else
		{
			// fix - unregistered connections receive ERROR, not NOTICE
			Write(t->fd,"ERROR :%s",s);
		}
	}
}

void Error(int status)
{
	signal(SIGALRM, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGABRT, SIG_IGN);
	signal(SIGSEGV, SIG_IGN);
	signal(SIGURG, SIG_IGN);
	signal(SIGKILL, SIG_IGN);
	log(DEFAULT,"*** fell down a pothole in the road to perfection ***");
	send_error("Error! Segmentation fault! save meeeeeeeeeeeeee *splat!*");
	Exit(status);
}

// this function counts all users connected, wether they are registered or NOT.
int usercnt(void)
{
	return clientlist.size();
}

// this counts only registered users, so that the percentages in /MAP don't mess up when users are sitting in an unregistered state
int registered_usercount(void)
{
	int c = 0;

	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if (i->second->registered == 7) c++;
	}

	return c;
}

int usercount_invisible(void)
{
	int c = 0;

	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if ((i->second->registered == 7) && (i->second->modebits & UM_INVISIBLE))
			c++;
	}

	return c;
}

int usercount_opers(void)
{
	int c = 0;

	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if (i->second->oper)
			c++;
	}
	return c;
}

int usercount_unknown(void)
{
	int c = 0;

	for (std::vector<userrec*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
	{
		userrec* t = (userrec*)(*i);
		if (t->registered != 7)
			c++;
	}

	return c;
}

long chancount(void)
{
	return chanlist.size();
}

long local_count()
{
	int c = 0;

	for (std::vector<userrec*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
	{
		userrec* t = (userrec*)(*i);
		if (t->registered == 7)
			c++;
	}

	return c;
}

void ShowMOTD(userrec *user)
{
	static char mbuf[MAXBUF];
	static char crud[MAXBUF];
	std::string WholeMOTD = "";

	if (!Config->MOTD.size())
	{
		WriteServ(user->fd,"422 %s :Message of the day file is missing.",user->nick);
		return;
	}

	snprintf(crud,MAXBUF,":%s 372 %s :- ", Config->ServerName, user->nick);
	snprintf(mbuf,MAXBUF,":%s 375 %s :- %s message of the day\r\n", Config->ServerName, user->nick, Config->ServerName);
	WholeMOTD = WholeMOTD + mbuf;

	for (unsigned int i = 0; i < Config->MOTD.size(); i++)
		WholeMOTD = WholeMOTD + std::string(crud) + Config->MOTD[i].c_str() + std::string("\r\n");

	snprintf(mbuf,MAXBUF,":%s 376 %s :End of message of the day.\r\n", Config->ServerName, user->nick);
	WholeMOTD = WholeMOTD + mbuf;

	// only one write operation
	if (Config->GetIOHook(user->port))
	{
		try
		{
			Config->GetIOHook(user->port)->OnRawSocketWrite(user->fd,(char*)WholeMOTD.c_str(),WholeMOTD.length());
		}
		catch (ModuleException& modexcept)
		{
			log(DEBUG,"Module exception caught: %s",modexcept.GetReason());
		}
	}
	else
	{
		user->AddWriteBuf(WholeMOTD);
	}

	ServerInstance->stats->statsSent += WholeMOTD.length();
}

void ShowRULES(userrec *user)
{
	if (!Config->RULES.size())
	{
		WriteServ(user->fd,"NOTICE %s :Rules file is missing.",user->nick);
		return;
	}
	WriteServ(user->fd,"NOTICE %s :%s rules",user->nick,Config->ServerName);

	for (unsigned int i = 0; i < Config->RULES.size(); i++)
		WriteServ(user->fd,"NOTICE %s :%s",user->nick,Config->RULES[i].c_str());

	WriteServ(user->fd,"NOTICE %s :End of %s rules.",user->nick,Config->ServerName);
}

// this returns 1 when all modules are satisfied that the user should be allowed onto the irc server
// (until this returns true, a user will block in the waiting state, waiting to connect up to the
// registration timeout maximum seconds)
bool AllModulesReportReady(userrec* user)
{
	if (!Config->global_implementation[I_OnCheckReady])
		return true;

	for (int i = 0; i <= MODCOUNT; i++)
	{
		if (Config->implement_lists[i][I_OnCheckReady])
		{
			int res = modules[i]->OnCheckReady(user);
			if (!res)
				return false;
		}
	}

	return true;
}

bool DirValid(char* dirandfile)
{
	char work[MAXBUF];
	char buffer[MAXBUF];
	char otherdir[MAXBUF];
	int p;

	strlcpy(work, dirandfile, MAXBUF);
	p = strlen(work);

	// we just want the dir
	while (*work)
	{
		if (work[p] == '/')
		{
			work[p] = '\0';
			break;
		}

		work[p--] = '\0';
	}

	// Get the current working directory
	if (getcwd(buffer, MAXBUF ) == NULL )
		return false;

	chdir(work);

	if (getcwd(otherdir, MAXBUF ) == NULL )
		return false;

	chdir(buffer);

	size_t t = strlen(work);

	if (strlen(otherdir) >= t)
	{
		otherdir[t] = '\0';

		if (!strcmp(otherdir,work))
		{
			return true;
		}

		return false;
	}
	else
	{
		return false;
	}
}

std::string GetFullProgDir(char** argv, int argc)
{
	char work[MAXBUF];
	char buffer[MAXBUF];
	char otherdir[MAXBUF];
	int p;

	strlcpy(work,argv[0],MAXBUF);
	p = strlen(work);

	// we just want the dir
	while (*work)
	{
		if (work[p] == '/')
		{
			work[p] = '\0';
			break;
		}

		work[p--] = '\0';
	}

	// Get the current working directory
	if (getcwd(buffer, MAXBUF) == NULL)
		return "";

	chdir(work);

	if (getcwd(otherdir, MAXBUF) == NULL)
		return "";

	chdir(buffer);
	return otherdir;
}

int InsertMode(std::string &output, const char* mode, unsigned short section)
{
	unsigned short currsection = 1;
	unsigned int pos = output.find("CHANMODES=", 0) + 10; // +10 for the length of "CHANMODES="
	
	if(section > 4 || section == 0)
	{
		log(DEBUG, "InsertMode: CHANMODES doesn't have a section %dh :/", section);
		return 0;
	}
	
	for(; pos < output.size(); pos++)
	{
		if(section == currsection)
			break;
			
		if(output[pos] == ',')
			currsection++;
	}
	
	output.insert(pos, mode);
	return 1;
}

bool IsValidChannelName(const char *chname)
{
	char *c;

	/* check for no name - don't check for !*chname, as if it is empty, it won't be '#'! */
	if (!chname || *chname != '#')
	{
		return false;
	}

	c = (char *)chname + 1;
	while (*c)
	{
		switch (*c)
		{
			case ' ':
			case ',':
			case 7:
				return false;
		}

		c++;
	}
		
	/* too long a name - note funky pointer arithmetic here. */
	if ((c - chname) > CHANMAX)
	{
			return false;
	}

	return true;
}

inline int charlcat(char* x,char y,int z)
{
	char* x__n = x;
	int v = 0;

	while(*x__n++)
		v++;

	if (v < z - 1)
	{
		*--x__n = y;
		*++x__n = 0;
	}

	return v;
}

bool charremove(char* mp, char remove)
{
	char* mptr = mp;
	bool shift_down = false;

	while (*mptr)
	{
		if (*mptr == remove)
		shift_down = true;

		if (shift_down)
			*mptr = *(mptr+1);

		mptr++;
	}

	return shift_down;
}
