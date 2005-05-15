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

#include "inspircd.h"
#include "inspircd_io.h"
#include "inspircd_util.h"
#include "inspircd_config.h"
#include <unistd.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <time.h>
#include <string>
#ifdef GCC3
#include <ext/hash_map>
#else
#include <hash_map>
#endif
#include <map>
#include <sstream>
#include <vector>
#include <deque>
#include "connection.h"
#include "users.h"
#include "servers.h"
#include "ctables.h"
#include "globals.h"
#include "modules.h"
#include "dynamic.h"
#include "wildcard.h"
#include "message.h"
#include "mode.h"
#include "xline.h"
#include "inspstring.h"
#include "helperfuncs.h"

#ifdef GCC3
#define nspace __gnu_cxx
#else
#define nspace std
#endif

extern int MODCOUNT;
extern std::vector<Module*, __single_client_alloc> modules;
extern std::vector<ircd_module*, __single_client_alloc> factory;

extern int LogLevel;
extern char ServerName[MAXBUF];
extern char Network[MAXBUF];
extern char ServerDesc[MAXBUF];
extern char AdminName[MAXBUF];
extern char AdminEmail[MAXBUF];
extern char AdminNick[MAXBUF];
extern char diepass[MAXBUF];
extern char restartpass[MAXBUF];
extern char motd[MAXBUF];
extern char rules[MAXBUF];
extern char list[MAXBUF];
extern char PrefixQuit[MAXBUF];
extern char DieValue[MAXBUF];

extern int debugging;
extern int WHOWAS_STALE;
extern int WHOWAS_MAX;
extern int DieDelay;
extern time_t startup_time;
extern int NetBufferSize;
int MaxWhoResults;
extern time_t nb_start;

extern std::vector<std::string, __single_client_alloc> module_names;

extern int boundPortCount;
extern int portCount;
extern int SERVERportCount;
extern int ports[MAXSOCKS];
extern int defaultRoute;

extern std::vector<long> auth_cookies;
extern std::stringstream config_f;

extern serverrec* me[32];

extern FILE *log_file;

extern time_t TIME;

using namespace std;

std::vector<ModeParameter, __single_client_alloc> custom_mode_params;

chanrec::chanrec()
{
	strcpy(name,"");
	strcpy(custom_modes,"");
	strcpy(topic,"");
	strcpy(setby,"");
	strcpy(key,"");
	created = topicset = limit = 0;
	binarymodes = 0;
	internal_userlist.clear();
}

void chanrec::SetCustomMode(char mode,bool mode_on)
{
	if (mode_on) {
		static char m[3];
		m[0] = mode;
		m[1] = '\0';
		if (!strchr(this->custom_modes,mode))
		{
			strlcat(custom_modes,m,MAXMODES);
		}
		log(DEBUG,"Custom mode %c set",mode);
	}
	else {

		std::string a = this->custom_modes;
		int pos = a.find(mode);
		a.erase(pos,1);
		strncpy(this->custom_modes,a.c_str(),MAXMODES);

		log(DEBUG,"Custom mode %c removed: modelist='%s'",mode,this->custom_modes);
		this->SetCustomModeParam(mode,"",false);
	}
}


void chanrec::SetCustomModeParam(char mode,char* parameter,bool mode_on)
{

	log(DEBUG,"SetCustomModeParam called");
	ModeParameter M;
	M.mode = mode;
	strlcpy(M.channel,this->name,CHANMAX);
	strlcpy(M.parameter,parameter,MAXBUF);
	if (mode_on)
	{
		log(DEBUG,"Custom mode parameter %c %s added",mode,parameter);
		custom_mode_params.push_back(M);
	}
	else
	{
		if (custom_mode_params.size())
		{
			for (vector<ModeParameter, __single_client_alloc>::iterator i = custom_mode_params.begin(); i < custom_mode_params.end(); i++)
			{
				if ((i->mode == mode) && (!strcasecmp(this->name,i->channel)))
				{
					log(DEBUG,"Custom mode parameter %c %s removed",mode,parameter);
					custom_mode_params.erase(i);
					return;
				}
			}
		}
		log(DEBUG,"*** BUG *** Attempt to remove non-existent mode parameter!");
	}
}

bool chanrec::IsCustomModeSet(char mode)
{
	log(DEBUG,"Checking ISCustomModeSet: %c %s",mode,this->custom_modes);
	return (strchr(this->custom_modes,mode) != 0);
}

std::string chanrec::GetModeParameter(char mode)
{
	if (custom_mode_params.size())
	{
		for (vector<ModeParameter, __single_client_alloc>::iterator i = custom_mode_params.begin(); i < custom_mode_params.end(); i++)
		{
			if ((i->mode == mode) && (!strcasecmp(this->name,i->channel)))
			{
				return i->parameter;
			}
		}
	}
	return "";
}

long chanrec::GetUserCounter()
{
	return (this->internal_userlist.size());
}

void chanrec::AddUser(char* castuser)
{
	internal_userlist.push_back(castuser);
	log(DEBUG,"Added casted user to channel's internal list");
}

void chanrec::DelUser(char* castuser)
{
	for (std::vector<char*, __single_client_alloc>::iterator a = internal_userlist.begin(); a < internal_userlist.end(); a++)
	{
		if (*a == castuser)
		{
			log(DEBUG,"Removed casted user from channel's internal list");
			internal_userlist.erase(a);
			return;
		}
	}
	log(DEBUG,"BUG BUG BUG! Attempt to remove an uncasted user from the internal list of %s!",name);
}

std::vector<char*, __single_client_alloc> *chanrec::GetUsers()
{
	return &internal_userlist;
}
