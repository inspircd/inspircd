#include "inspircd.h"
#include "inspircd_io.h"
#include "inspircd_util.h"
#include "inspircd_config.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <cstdio>
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
#include <errno.h>
#include <deque>
#include <errno.h>
#include <unistd.h>
#include <sched.h>
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
#include "commands.h"

#ifdef GCC3
#define nspace __gnu_cxx
#else
#define nspace std
#endif

using namespace std;

extern int MODCOUNT;
extern vector<Module*> modules;
extern vector<ircd_module*> factory;

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
extern int MaxWhoResults;
extern time_t nb_start;

extern std::vector<int> fd_reap;
extern std::vector<std::string> module_names;

extern int boundPortCount;
extern int portCount;
extern int UDPportCount;
extern int ports[MAXSOCKS];
extern int defaultRoute;

extern std::vector<long> auth_cookies;
extern std::stringstream config_f;

extern serverrec* me[32];

extern FILE *log_file;


namespace nspace
{
	template<> struct nspace::hash<in_addr>
	{
		size_t operator()(const struct in_addr &a) const
		{
			size_t q;
			memcpy(&q,&a,sizeof(size_t));
			return q;
		}
	};

	template<> struct nspace::hash<string>
	{
		size_t operator()(const string &s) const
		{
			char a[MAXBUF];
			static struct hash<const char *> strhash;
			strcpy(a,s.c_str());
			strlower(a);
			return strhash(a);
		}
	};
}	


struct StrHashComp
{

	bool operator()(const string& s1, const string& s2) const
	{
		char a[MAXBUF],b[MAXBUF];
		strcpy(a,s1.c_str());
		strcpy(b,s2.c_str());
		return (strcasecmp(a,b) == 0);
	}

};

struct InAddr_HashComp
{

	bool operator()(const in_addr &s1, const in_addr &s2) const
	{
		size_t q;
		size_t p;
		
		memcpy(&q,&s1,sizeof(size_t));
		memcpy(&p,&s2,sizeof(size_t));
		
		return (q == p);
	}

};


typedef nspace::hash_map<std::string, userrec*, nspace::hash<string>, StrHashComp> user_hash;
typedef nspace::hash_map<std::string, chanrec*, nspace::hash<string>, StrHashComp> chan_hash;
typedef nspace::hash_map<in_addr,string*, nspace::hash<in_addr>, InAddr_HashComp> address_cache;
typedef std::deque<command_t> command_table;


extern user_hash clientlist;
extern chan_hash chanlist;
extern user_hash whowas;
extern command_table cmdlist;
extern file_cache MOTD;
extern file_cache RULES;
extern address_cache IP;


// class type for holding an extended mode character - internal to core

class ExtMode : public classbase
{
public:
	char modechar;
	int type;
	int params_when_on;
	int params_when_off;
	bool needsoper;
	bool list;
	ExtMode(char mc, int ty, bool oper, int p_on, int p_off) : modechar(mc), type(ty), needsoper(oper), params_when_on(p_on), params_when_off(p_off) { };
};                                     

typedef std::vector<ExtMode> ExtModeList;
typedef ExtModeList::iterator ExtModeListIter;


ExtModeList EMode;

// returns true if an extended mode character is in use
bool ModeDefined(char modechar, int type)
{
	log(DEBUG,"Size of extmodes vector is %d",EMode.size());
	for (ExtModeListIter i = EMode.begin(); i < EMode.end(); i++)
	{
		if ((i->modechar == modechar) && (i->type == type))
		{
			return true;
		}
	}
	return false;
}

bool ModeIsListMode(char modechar, int type)
{
	log(DEBUG,"Size of extmodes vector is %d",EMode.size());
	for (ExtModeListIter i = EMode.begin(); i < EMode.end(); i++)
	{
		if ((i->modechar == modechar) && (i->type == type) && (i->list == true))
		{
			return true;
		}
	}
	return false;
}

bool ModeDefinedOper(char modechar, int type)
{
	log(DEBUG,"Size of extmodes vector is %d",EMode.size());
	for (ExtModeListIter i = EMode.begin(); i < EMode.end(); i++)
	{
		if ((i->modechar == modechar) && (i->type == type) && (i->needsoper == true))
		{
			return true;
		}
	}
	return false;
}

// returns number of parameters for a custom mode when it is switched on
int ModeDefinedOn(char modechar, int type)
{
	for (ExtModeListIter i = EMode.begin(); i < EMode.end(); i++)
	{
		if ((i->modechar == modechar) && (i->type == type))
		{
			return i->params_when_on;
		}
	}
	return 0;
}

// returns number of parameters for a custom mode when it is switched on
int ModeDefinedOff(char modechar, int type)
{
	for (ExtModeListIter i = EMode.begin(); i < EMode.end(); i++)
	{
		if ((i->modechar == modechar) && (i->type == type))
		{
			return i->params_when_off;
		}
	}
	return 0;
}

// returns true if an extended mode character is in use
bool DoAddExtendedMode(char modechar, int type, bool requires_oper, int params_on, int params_off)
{
	if (ModeDefined(modechar,type)) {
		return false;
	}
	EMode.push_back(ExtMode(modechar,type,requires_oper,params_on,params_off));
	return true;
}

// turns a mode into a listmode
void ModeMakeList(char modechar)
{
	for (ExtModeListIter i = EMode.begin(); i < EMode.end(); i++)
	{
		if ((i->modechar == modechar) && (i->type == MT_CHANNEL))
		{
			i->list = true;
			return;
		}
	}
	return;
}

// version is a simple class for holding a modules version number

Version::Version(int major, int minor, int revision, int build) : Major(major), Minor(minor), Revision(revision), Build(build) { };

// admin is a simple class for holding a server's administrative info

Admin::Admin(std::string name, std::string email, std::string nick) : Name(name), Email(email), Nick(nick) { };

Module::Module() { }
Module::~Module() { }
void Module::OnUserConnect(userrec* user) { }
void Module::OnUserQuit(userrec* user) { }
void Module::OnUserJoin(userrec* user, chanrec* channel) { }
void Module::OnUserPart(userrec* user, chanrec* channel) { }
void Module::OnPacketTransmit(char *p) { }
void Module::OnPacketReceive(char *p) { }
void Module::OnRehash() { }
void Module::OnServerRaw(std::string &raw, bool inbound, userrec* user) { }
int Module::OnUserPreJoin(userrec* user, chanrec* chan, const char* cname) { return 0; }
int Module::OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params) { return false; }
Version Module::GetVersion() { return Version(1,0,0,0); }
void Module::OnOper(userrec* user) { };
void Module::OnInfo(userrec* user) { };
void Module::OnWhois(userrec* source, userrec* dest) { };
int Module::OnUserPreMessage(userrec* user,void* dest,int target_type, std::string text) { return 0; };
int Module::OnUserPreNotice(userrec* user,void* dest,int target_type, std::string text) { return 0; };
int Module::OnUserPreNick(userrec* user, std::string newnick) { return 0; };
int Module::OnAccessCheck(userrec* source,userrec* dest,chanrec* channel,int access_type) { return ACR_DEFAULT; };
string_list Module::OnUserSync(userrec* user) { string_list empty; return empty; }
string_list Module::OnChannelSync(chanrec* chan) { string_list empty; return empty; }


// server is a wrapper class that provides methods to all of the C-style
// exports in the core
//

Server::Server()
{
}

Server::~Server()
{
}

void Server::SendOpers(std::string s)
{
	WriteOpers("%s",s.c_str());
}

bool Server::MatchText(std::string sliteral, std::string spattern)
{
	char literal[MAXBUF],pattern[MAXBUF];
	strncpy(literal,sliteral.c_str(),MAXBUF);
	strncpy(pattern,spattern.c_str(),MAXBUF);
	return match(literal,pattern);
}

void Server::SendToModeMask(std::string modes, int flags, std::string text)
{
	WriteMode(modes.c_str(),flags,"%s",text.c_str());
}

chanrec* Server::JoinUserToChannel(userrec* user, std::string cname, std::string key)
{
	return add_channel(user,cname.c_str(),key.c_str(),true);
}

chanrec* Server::PartUserFromChannel(userrec* user, std::string cname, std::string reason)
{
	return del_channel(user,cname.c_str(),reason.c_str(),false);
}

chanuserlist Server::GetUsers(chanrec* chan)
{
	chanuserlist userl;
	userl.clear();
  	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if (i->second)
		{
			if (has_channel(i->second,chan))
			{
				if (isnick(i->second->nick))
				{
					userl.push_back(i->second);
				}
			}
		}
	}
	return userl;
}
void Server::ChangeUserNick(userrec* user, std::string nickname)
{
	force_nickchange(user,nickname.c_str());
}

void Server::QuitUser(userrec* user, std::string reason)
{
	send_network_quit(user->nick,reason.c_str());
	kill_link(user,reason.c_str());
}

bool Server::IsUlined(std::string server)
{
	return is_uline(server.c_str());
}

void Server::CallCommandHandler(std::string commandname, char** parameters, int pcnt, userrec* user)
{
	call_handler(commandname.c_str(),parameters,pcnt,user);
}

void Server::Log(int level, std::string s)
{
	log(level,"%s",s.c_str());
}

void Server::AddCommand(char* cmd, handlerfunc f, char flags, int minparams)
{
	createcommand(cmd,f,flags,minparams);
}

void Server::SendMode(char **parameters, int pcnt, userrec *user)
{
	server_mode(parameters,pcnt,user);
}

void Server::Send(int Socket, std::string s)
{
	Write(Socket,"%s",s.c_str());
}

void Server::SendServ(int Socket, std::string s)
{
	WriteServ(Socket,"%s",s.c_str());
}

void Server::SendFrom(int Socket, userrec* User, std::string s)
{
	WriteFrom(Socket,User,"%s",s.c_str());
}

void Server::SendTo(userrec* Source, userrec* Dest, std::string s)
{
	if (!Source)
	{
		// if source is NULL, then the message originates from the local server
		Write(Dest->fd,":%s %s",this->GetServerName().c_str(),s.c_str());
	}
	else
	{
		// otherwise it comes from the user specified
		WriteTo(Source,Dest,"%s",s.c_str());
	}
}

void Server::SendChannel(userrec* User, chanrec* Channel, std::string s,bool IncludeSender)
{
	if (IncludeSender)
	{
		WriteChannel(Channel,User,"%s",s.c_str());
	}
	else
	{
		ChanExceptSender(Channel,User,"%s",s.c_str());
	}
}

bool Server::CommonChannels(userrec* u1, userrec* u2)
{
	return (common_channels(u1,u2) != 0);
}

void Server::SendCommon(userrec* User, std::string text,bool IncludeSender)
{
	if (IncludeSender)
	{
		WriteCommon(User,"%s",text.c_str());
	}
	else
	{
		WriteCommonExcept(User,"%s",text.c_str());
	}
}

void Server::SendWallops(userrec* User, std::string text)
{
	WriteWallOps(User,false,"%s",text.c_str());
}

void Server::ChangeHost(userrec* user, std::string host)
{
	ChangeDisplayedHost(user,host.c_str());
}

void Server::ChangeGECOS(userrec* user, std::string gecos)
{
	ChangeName(user,gecos.c_str());
}

bool Server::IsNick(std::string nick)
{
	return (isnick(nick.c_str()) != 0);
}

userrec* Server::FindNick(std::string nick)
{
	return Find(nick);
}

chanrec* Server::FindChannel(std::string channel)
{
	return FindChan(channel.c_str());
}

std::string Server::ChanMode(userrec* User, chanrec* Chan)
{
	return cmode(User,Chan);
}

bool Server::IsOnChannel(userrec* User, chanrec* Chan)
{
	return has_channel(User,Chan);
}

std::string Server::GetServerName()
{
	return getservername();
}

std::string Server::GetNetworkName()
{
	return getnetworkname();
}

Admin Server::GetAdmin()
{
	return Admin(getadminname(),getadminemail(),getadminnick());
}



bool Server::AddExtendedMode(char modechar, int type, bool requires_oper, int params_when_on, int params_when_off)
{
	if (type == MT_SERVER)
	{
		log(DEBUG,"*** API ERROR *** Modes of type MT_SERVER are reserved for future expansion");
		return false;
	}
	if (((params_when_on>0) || (params_when_off>0)) && (type == MT_CLIENT))
	{
		log(DEBUG,"*** API ERROR *** Parameters on MT_CLIENT modes are not supported");
		return false;
	}
	if ((params_when_on>1) || (params_when_off>1))
	{
		log(DEBUG,"*** API ERROR *** More than one parameter for an MT_CHANNEL mode is not yet supported");
		return false;
	}
	return DoAddExtendedMode(modechar,type,requires_oper,params_when_on,params_when_off);
}

bool Server::AddExtendedListMode(char modechar)
{
	bool res = DoAddExtendedMode(modechar,MT_CHANNEL,false,1,1);
	if (res)
		ModeMakeList(modechar);
	return res;
}

int Server::CountUsers(chanrec* c)
{
	return usercount(c);
}


ConfigReader::ConfigReader()
{
	this->cache = new std::stringstream(std::stringstream::in | std::stringstream::out);
	this->readerror = LoadConf(CONFIG_FILE,this->cache);
	if (!this->readerror)
		this->error = CONF_FILE_NOT_FOUND;
}


ConfigReader::~ConfigReader()
{
	if (this->cache)
		delete this->cache;
}


ConfigReader::ConfigReader(std::string filename)
{
	this->cache = new std::stringstream(std::stringstream::in | std::stringstream::out);
	this->readerror = LoadConf(filename.c_str(),this->cache);
	if (!this->readerror)
		this->error = CONF_FILE_NOT_FOUND;
};

std::string ConfigReader::ReadValue(std::string tag, std::string name, int index)
{
	char val[MAXBUF];
	char t[MAXBUF];
	char n[MAXBUF];
	strncpy(t,tag.c_str(),MAXBUF);
	strncpy(n,name.c_str(),MAXBUF);
	int res = ReadConf(cache,t,n,index,val);
	if (!res)
	{
		this->error = CONF_VALUE_NOT_FOUND;
		return "";
	}
	return std::string(val);
}

bool ConfigReader::ReadFlag(std::string tag, std::string name, int index)
{
	char val[MAXBUF];
	char t[MAXBUF];
	char n[MAXBUF];
	strncpy(t,tag.c_str(),MAXBUF);
	strncpy(n,name.c_str(),MAXBUF);
	int res = ReadConf(cache,t,n,index,val);
	if (!res)
	{
		this->error = CONF_VALUE_NOT_FOUND;
		return false;
	}
	std::string s = val;
	return ((s == "yes") || (s == "YES") || (s == "true") || (s == "TRUE") || (s == "1"));
}

long ConfigReader::ReadInteger(std::string tag, std::string name, int index, bool needs_unsigned)
{
	char val[MAXBUF];
	char t[MAXBUF];
	char n[MAXBUF];
	strncpy(t,tag.c_str(),MAXBUF);
	strncpy(n,name.c_str(),MAXBUF);
	int res = ReadConf(cache,t,n,index,val);
	if (!res)
	{
		this->error = CONF_VALUE_NOT_FOUND;
		return 0;
	}
	for (int i = 0; i < strlen(val); i++)
	{
		if (!isdigit(val[i]))
		{
			this->error = CONF_NOT_A_NUMBER;
			return 0;
		}
	}
	if ((needs_unsigned) && (atoi(val)<0))
	{
		this->error = CONF_NOT_UNSIGNED;
		return 0;
	}
	return atoi(val);
}

long ConfigReader::GetError()
{
	long olderr = this->error;
	this->error = 0;
	return olderr;
}


int ConfigReader::Enumerate(std::string tag)
{
	return EnumConf(cache,tag.c_str());
}

int ConfigReader::EnumerateValues(std::string tag, int index)
{
	return EnumValues(cache, tag.c_str(), index);
}

bool ConfigReader::Verify()
{
	return this->readerror;
}


FileReader::FileReader(std::string filename)
{
	file_cache c;
	readfile(c,filename.c_str());
	this->fc = c;
}

FileReader::FileReader()
{
}

void FileReader::LoadFile(std::string filename)
{
	file_cache c;
	readfile(c,filename.c_str());
	this->fc = c;
}


FileReader::~FileReader()
{
}

bool FileReader::Exists()
{
	if (fc.size() == 0)
	{
		return(false);
	}
	else
	{
		return(true);
	}
}

std::string FileReader::GetLine(int x)
{
	if ((x<0) || (x>fc.size()))
		return "";
	return fc[x];
}

int FileReader::FileSize()
{
	return fc.size();
}


std::vector<Module*> modules(255);
std::vector<ircd_module*> factory(255);

int MODCOUNT  = -1;


