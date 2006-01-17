/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
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
#include "inspircd.h"
#include "inspircd_io.h"
#include <unistd.h>
#include <sys/errno.h>
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
#include "socket.h"
#include "socketengine.h"
#include "typedefs.h"
#include "modules.h"
#include "command_parse.h"

extern ServerConfig *Config;
extern InspIRCd* ServerInstance;
extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern std::vector<InspSocket*> module_sockets;
extern std::vector<userrec*> local_users;
extern time_t TIME;
class Server;
extern userrec* fd_ref_table[MAX_DESCRIPTORS];

extern user_hash clientlist;
extern chan_hash chanlist;
extern command_table cmdlist;
ExtModeList EMode;

// returns true if an extended mode character is in use
bool ModeDefined(char modechar, int type)
{
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

Version::Version(int major, int minor, int revision, int build, int flags) : Major(major), Minor(minor), Revision(revision), Build(build), Flags(flags) { };

// admin is a simple class for holding a server's administrative info

Admin::Admin(std::string name, std::string email, std::string nick) : Name(name), Email(email), Nick(nick) { };

Request::Request(char* anydata, Module* src, Module* dst) : data(anydata), source(src), dest(dst) { };

char* Request::GetData()
{
        return this->data;
}

Module* Request::GetSource()
{
        return this->source;
}

Module* Request::GetDest()
{
        return this->dest;
}

char* Request::Send()
{
        if (this->dest)
        {
                return dest->OnRequest(this);
        }
        else
        {
                return NULL;
        }
}

Event::Event(char* anydata, Module* src, std::string eventid) : data(anydata), source(src), id(eventid) { };

char* Event::GetData()
{
        return this->data;
}

Module* Event::GetSource()
{
        return this->source;
}

char* Event::Send()
{
        FOREACH_MOD(I_OnEvent,OnEvent(this));
        return NULL;
}

std::string Event::GetEventID()
{
        return this->id;
}


// These declarations define the behavours of the base class Module (which does nothing at all)

		Module::Module(Server* Me) { }
		Module::~Module() { }
void		Module::OnUserConnect(userrec* user) { }
void		Module::OnUserQuit(userrec* user, std::string message) { }
void		Module::OnUserDisconnect(userrec* user) { }
void		Module::OnUserJoin(userrec* user, chanrec* channel) { }
void		Module::OnUserPart(userrec* user, chanrec* channel, std::string partmessage) { }
void		Module::OnRehash(std::string parameter) { }
void		Module::OnServerRaw(std::string &raw, bool inbound, userrec* user) { }
int		Module::OnUserPreJoin(userrec* user, chanrec* chan, const char* cname) { return 0; }
int		Module::OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params) { return false; }
void		Module::OnMode(userrec* user, void* dest, int target_type, std::string text) { };
Version		Module::GetVersion() { return Version(1,0,0,0,VF_VENDOR); }
void		Module::OnOper(userrec* user, std::string opertype) { };
void		Module::OnInfo(userrec* user) { };
void		Module::OnWhois(userrec* source, userrec* dest) { };
int		Module::OnUserPreInvite(userrec* source,userrec* dest,chanrec* channel) { return 0; };
int		Module::OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text) { return 0; };
int		Module::OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text) { return 0; };
int		Module::OnUserPreNick(userrec* user, std::string newnick) { return 0; };
void		Module::OnUserPostNick(userrec* user, std::string oldnick) { };
int		Module::OnAccessCheck(userrec* source,userrec* dest,chanrec* channel,int access_type) { return ACR_DEFAULT; };
void		Module::On005Numeric(std::string &output) { };
int		Module::OnKill(userrec* source, userrec* dest, std::string reason) { return 0; };
void		Module::OnLoadModule(Module* mod,std::string name) { };
void		Module::OnUnloadModule(Module* mod,std::string name) { };
void		Module::OnBackgroundTimer(time_t curtime) { };
void		Module::OnSendList(userrec* user, chanrec* channel, char mode) { };
int		Module::OnPreCommand(std::string command, char **parameters, int pcnt, userrec *user, bool validated) { return 0; };
bool		Module::OnCheckReady(userrec* user) { return true; };
void		Module::OnUserRegister(userrec* user) { };
int		Module::OnUserPreKick(userrec* source, userrec* user, chanrec* chan, std::string reason) { return 0; };
void		Module::OnUserKick(userrec* source, userrec* user, chanrec* chan, std::string reason) { };
int		Module::OnRawMode(userrec* user, chanrec* chan, char mode, std::string param, bool adding, int pcnt) { return 0; };
int		Module::OnCheckInvite(userrec* user, chanrec* chan) { return 0; };
int		Module::OnCheckKey(userrec* user, chanrec* chan, std::string keygiven) { return 0; };
int		Module::OnCheckLimit(userrec* user, chanrec* chan) { return 0; };
int		Module::OnCheckBan(userrec* user, chanrec* chan) { return 0; };
int		Module::OnStats(char symbol, userrec* user) { return 0; };
int		Module::OnChangeLocalUserHost(userrec* user, std::string newhost) { return 0; };
int		Module::OnChangeLocalUserGECOS(userrec* user, std::string newhost) { return 0; };
int		Module::OnLocalTopicChange(userrec* user, chanrec* chan, std::string topic) { return 0; };
void		Module::OnEvent(Event* event) { return; };
char*		Module::OnRequest(Request* request) { return NULL; };
int		Module::OnOperCompare(std::string password, std::string input) { return 0; };
void		Module::OnGlobalOper(userrec* user) { };
void		Module::OnGlobalConnect(userrec* user) { };
int		Module::OnAddBan(userrec* source, chanrec* channel,std::string banmask) { return 0; };
int		Module::OnDelBan(userrec* source, chanrec* channel,std::string banmask) { return 0; };
void		Module::OnRawSocketAccept(int fd, std::string ip, int localport) { };
int		Module::OnRawSocketWrite(int fd, char* buffer, int count) { return 0; };
void		Module::OnRawSocketClose(int fd) { };
int		Module::OnRawSocketRead(int fd, char* buffer, unsigned int count, int &readresult) { return 0; };
void		Module::OnUserMessage(userrec* user, void* dest, int target_type, std::string text) { };
void		Module::OnUserNotice(userrec* user, void* dest, int target_type, std::string text) { };
void 		Module::OnRemoteKill(userrec* source, userrec* dest, std::string reason) { };
void		Module::OnUserInvite(userrec* source,userrec* dest,chanrec* channel) { };
void		Module::OnPostLocalTopicChange(userrec* user, chanrec* chan, std::string topic) { };
void		Module::OnGetServerDescription(std::string servername,std::string &description) { };
void		Module::OnSyncUser(userrec* user, Module* proto, void* opaque) { };
void		Module::OnSyncChannel(chanrec* chan, Module* proto, void* opaque) { };
void		Module::ProtoSendMode(void* opaque, int target_type, void* target, std::string modeline) { };
void		Module::OnSyncChannelMetaData(chanrec* chan, Module* proto,void* opaque, std::string extname) { };
void		Module::OnSyncUserMetaData(userrec* user, Module* proto,void* opaque, std::string extname) { };
void		Module::OnDecodeMetaData(int target_type, void* target, std::string extname, std::string extdata) { };
void		Module::ProtoSendMetaData(void* opaque, int target_type, void* target, std::string extname, std::string extdata) { };
void		Module::OnWallops(userrec* user, std::string text) { };
void		Module::OnChangeHost(userrec* user, std::string newhost) { };
void		Module::OnChangeName(userrec* user, std::string gecos) { };
void		Module::OnAddGLine(long duration, userrec* source, std::string reason, std::string hostmask) { };
void		Module::OnAddZLine(long duration, userrec* source, std::string reason, std::string ipmask) { };
void		Module::OnAddKLine(long duration, userrec* source, std::string reason, std::string hostmask) { };
void		Module::OnAddQLine(long duration, userrec* source, std::string reason, std::string nickmask) { };
void		Module::OnAddELine(long duration, userrec* source, std::string reason, std::string hostmask) { };
void		Module::OnDelGLine(userrec* source, std::string hostmask) { };
void		Module::OnDelZLine(userrec* source, std::string ipmask) { };
void		Module::OnDelKLine(userrec* source, std::string hostmask) { };
void		Module::OnDelQLine(userrec* source, std::string nickmask) { };
void		Module::OnDelELine(userrec* source, std::string hostmask) { };
void 		Module::OnCleanup(int target_type, void* item) { };
void		Module::Implements(char* Implements) { for (int j = 0; j < 255; j++) Implements[j] = 0; };
Priority	Module::Prioritize() { return PRIORITY_DONTCARE; }

/* server is a wrapper class that provides methods to all of the C-style
 * exports in the core
 */

Server::Server()
{
}

Server::~Server()
{
}

void Server::AddSocket(InspSocket* sock)
{
	module_sockets.push_back(sock);
}

void Server::RemoveSocket(InspSocket* sock)
{
        for (std::vector<InspSocket*>::iterator a = module_sockets.begin(); a < module_sockets.end(); a++)
        {
                InspSocket* s = (InspSocket*)*a;
                if (s == sock)
                {
			log(DEBUG,"Forcibly removed socket");
                        ServerInstance->SE->DelFd(s->GetFd());
                        s->Close();
                        module_sockets.erase(a);
                        delete s;
                        return;
                }
        }
}

void Server::RehashServer()
{
	WriteOpers("*** Rehashing config file");
	Config->Read(false,NULL);
}

ServerConfig* Server::GetConfig()
{
	return Config;
}

std::string Server::GetVersion()
{
	return ServerInstance->GetVersionString();
}

void Server::DelSocket(InspSocket* sock)
{
	for (std::vector<InspSocket*>::iterator a = module_sockets.begin(); a < module_sockets.end(); a++)
	{
		if (*a == sock)
		{
			module_sockets.erase(a);
			return;
		}
	}
}

void Server::SendOpers(std::string s)
{
	WriteOpers("%s",s.c_str());
}

bool Server::MatchText(std::string sliteral, std::string spattern)
{
	char literal[MAXBUF],pattern[MAXBUF];
	strlcpy(literal,sliteral.c_str(),MAXBUF);
	strlcpy(pattern,spattern.c_str(),MAXBUF);
	return match(literal,pattern);
}

void Server::SendToModeMask(std::string modes, int flags, std::string text)
{
	WriteMode(modes.c_str(),flags,"%s",text.c_str());
}

chanrec* Server::JoinUserToChannel(userrec* user, std::string cname, std::string key)
{
	return add_channel(user,cname.c_str(),key.c_str(),false);
}

chanrec* Server::PartUserFromChannel(userrec* user, std::string cname, std::string reason)
{
	return del_channel(user,cname.c_str(),reason.c_str(),false);
}

chanuserlist Server::GetUsers(chanrec* chan)
{
	chanuserlist userl;
	userl.clear();
	std::map<char*,char*> *list = chan->GetUsers();
  	for (std::map<char*,char*>::iterator i = list->begin(); i != list->end(); i++)
	{
		char* o = i->second;
		userl.push_back((userrec*)o);
	}
	return userl;
}
void Server::ChangeUserNick(userrec* user, std::string nickname)
{
	force_nickchange(user,nickname.c_str());
}

void Server::QuitUser(userrec* user, std::string reason)
{
	kill_link(user,reason.c_str());
}

bool Server::IsUlined(std::string server)
{
	return is_uline(server.c_str());
}

void Server::CallCommandHandler(std::string commandname, char** parameters, int pcnt, userrec* user)
{
	ServerInstance->Parser->CallHandler(commandname,parameters,pcnt,user);
}

bool Server::IsValidModuleCommand(std::string commandname, int pcnt, userrec* user)
{
	return ServerInstance->Parser->IsValidCommand(commandname, pcnt, user);
}

void Server::Log(int level, std::string s)
{
	log(level,"%s",s.c_str());
}

void Server::AddCommand(command_t *f)
{
	ServerInstance->Parser->CreateCommand(f);
}

void Server::SendMode(char **parameters, int pcnt, userrec *user)
{
	ServerInstance->ModeGrok->ServerMode(parameters,pcnt,user);
}

void Server::Send(int Socket, std::string s)
{
	Write_NoFormat(Socket,s.c_str());
}

void Server::SendServ(int Socket, std::string s)
{
	WriteServ_NoFormat(Socket,s.c_str());
}

void Server::SendFrom(int Socket, userrec* User, std::string s)
{
	WriteFrom_NoFormat(Socket,User,s.c_str());
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
		WriteTo_NoFormat(Source,Dest,s.c_str());
	}
}

void Server::SendChannelServerNotice(std::string ServName, chanrec* Channel, std::string text)
{
	WriteChannelWithServ_NoFormat((char*)ServName.c_str(), Channel, text.c_str());
}

void Server::SendChannel(userrec* User, chanrec* Channel, std::string s,bool IncludeSender)
{
	if (IncludeSender)
	{
		WriteChannel_NoFormat(Channel,User,s.c_str());
	}
	else
	{
		ChanExceptSender_NoFormat(Channel,User,s.c_str());
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
		WriteCommon_NoFormat(User,text.c_str());
	}
	else
	{
		WriteCommonExcept_NoFormat(User,text.c_str());
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

userrec* Server::FindDescriptor(int socket)
{
	return (socket < 65536 ? fd_ref_table[socket] : NULL);
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
	return Config->ServerName;
}

std::string Server::GetNetworkName()
{
	return Config->Network;
}

std::string Server::GetServerDescription()
{
	return Config->ServerDesc;
}

Admin Server::GetAdmin()
{
	return Admin(Config->AdminName,Config->AdminEmail,Config->AdminNick);
}



bool Server::AddExtendedMode(char modechar, int type, bool requires_oper, int params_when_on, int params_when_off)
{
	if (((modechar >= 'A') && (modechar <= 'Z')) || ((modechar >= 'a') && (modechar <= 'z')))
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
	else
	{
		log(DEBUG,"*** API ERROR *** Muppet modechar detected.");
	}
	return false;
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


bool Server::UserToPseudo(userrec* user,std::string message)
{
	unsigned int old_fd = user->fd;
	Write(old_fd,"ERROR :Closing link (%s@%s) [%s]",user->ident,user->host,message.c_str());
	user->FlushWriteBuf();
	user->ClearBuffer();
	user->fd = FD_MAGIC_NUMBER;

	if (find(local_users.begin(),local_users.end(),user) != local_users.end())
	{
		local_users.erase(find(local_users.begin(),local_users.end(),user));
		log(DEBUG,"Delete local user");
	}

	ServerInstance->SE->DelFd(old_fd);
        shutdown(old_fd,2);
        close(old_fd);
	return true;
}

bool Server::PseudoToUser(userrec* alive,userrec* zombie,std::string message)
{
	log(DEBUG,"PseudoToUser");
	zombie->fd = alive->fd;
	FOREACH_MOD(I_OnUserQuit,OnUserQuit(alive,message));
	alive->fd = FD_MAGIC_NUMBER;
	alive->FlushWriteBuf();
	alive->ClearBuffer();
	Write(zombie->fd,":%s!%s@%s NICK %s",alive->nick,alive->ident,alive->host,zombie->nick);
	kill_link(alive,message.c_str());

        if (find(local_users.begin(),local_users.end(),alive) != local_users.end())
        {
		local_users.erase(find(local_users.begin(),local_users.end(),alive));
		log(DEBUG,"Delete local user");
        }

	fd_ref_table[zombie->fd] = zombie;
        for (unsigned int i = 0; i < zombie->chans.size(); i++)
        {
                if (zombie->chans[i].channel != NULL)
                {
                        if (zombie->chans[i].channel->name)
                        {
				chanrec* Ptr = zombie->chans[i].channel;
				WriteFrom(zombie->fd,zombie,"JOIN %s",Ptr->name);
	                        if (Ptr->topicset)
                        	{
                                	WriteServ(zombie->fd,"332 %s %s :%s", zombie->nick, Ptr->name, Ptr->topic);
                                	WriteServ(zombie->fd,"333 %s %s %s %d", zombie->nick, Ptr->name, Ptr->setby, Ptr->topicset);
                        	}
                        	userlist(zombie,Ptr);
                        	WriteServ(zombie->fd,"366 %s %s :End of /NAMES list.", zombie->nick, Ptr->name);

                        }
                }
        }
	if ((find(local_users.begin(),local_users.end(),zombie) == local_users.end()) && (zombie->fd != FD_MAGIC_NUMBER))
		local_users.push_back(zombie);

	return true;
}

void Server::AddGLine(long duration, std::string source, std::string reason, std::string hostmask)
{
	add_gline(duration, source.c_str(), reason.c_str(), hostmask.c_str());
}

void Server::AddQLine(long duration, std::string source, std::string reason, std::string nickname)
{
	add_qline(duration, source.c_str(), reason.c_str(), nickname.c_str());
}

void Server::AddZLine(long duration, std::string source, std::string reason, std::string ipaddr)
{
	add_zline(duration, source.c_str(), reason.c_str(), ipaddr.c_str());
}

void Server::AddKLine(long duration, std::string source, std::string reason, std::string hostmask)
{
	add_kline(duration, source.c_str(), reason.c_str(), hostmask.c_str());
}

void Server::AddELine(long duration, std::string source, std::string reason, std::string hostmask)
{
	add_eline(duration, source.c_str(), reason.c_str(), hostmask.c_str());
}

bool Server::DelGLine(std::string hostmask)
{
	return del_gline(hostmask.c_str());
}

bool Server::DelQLine(std::string nickname)
{
	return del_qline(nickname.c_str());
}

bool Server::DelZLine(std::string ipaddr)
{
	return del_zline(ipaddr.c_str());
}

bool Server::DelKLine(std::string hostmask)
{
	return del_kline(hostmask.c_str());
}

bool Server::DelELine(std::string hostmask)
{
	return del_eline(hostmask.c_str());
}

long Server::CalcDuration(std::string delta)
{
	return duration(delta.c_str());
}

bool Server::IsValidMask(std::string mask)
{
	const char* dest = mask.c_str();
        if (strchr(dest,'!')==0)
                return false;
        if (strchr(dest,'@')==0)
                return false;
        for (unsigned int i = 0; i < strlen(dest); i++)
                if (dest[i] < 32)
                        return false;
        for (unsigned int i = 0; i < strlen(dest); i++)
                if (dest[i] > 126)
                        return false;
        unsigned int c = 0;
        for (unsigned int i = 0; i < strlen(dest); i++)
                if (dest[i] == '!')
                        c++;
        if (c>1)
                return false;
        c = 0;
        for (unsigned int i = 0; i < strlen(dest); i++)
                if (dest[i] == '@')
                        c++;
        if (c>1)
                return false;

	return true;
}

Module* Server::FindModule(std::string name)
{
	for (int i = 0; i <= MODCOUNT; i++)
	{
		if (Config->module_names[i] == name)
		{
			return modules[i];
		}
	}
	return NULL;
}

ConfigReader::ConfigReader()
{
	Config->ClearStack();
	this->cache = new std::stringstream(std::stringstream::in | std::stringstream::out);
	this->errorlog = new std::stringstream(std::stringstream::in | std::stringstream::out);
	this->readerror = Config->LoadConf(CONFIG_FILE,this->cache,this->errorlog);
	if (!this->readerror)
		this->error = CONF_FILE_NOT_FOUND;
}


ConfigReader::~ConfigReader()
{
	if (this->cache)
		delete this->cache;
	if (this->errorlog)
		delete this->errorlog;
}


ConfigReader::ConfigReader(std::string filename)
{
	Config->ClearStack();
	this->cache = new std::stringstream(std::stringstream::in | std::stringstream::out);
	this->errorlog = new std::stringstream(std::stringstream::in | std::stringstream::out);
	this->readerror = Config->LoadConf(filename.c_str(),this->cache,this->errorlog);
	if (!this->readerror)
		this->error = CONF_FILE_NOT_FOUND;
};

std::string ConfigReader::ReadValue(std::string tag, std::string name, int index)
{
	char val[MAXBUF];
	char t[MAXBUF];
	char n[MAXBUF];
	strlcpy(t,tag.c_str(),MAXBUF);
	strlcpy(n,name.c_str(),MAXBUF);
	int res = Config->ReadConf(cache,t,n,index,val);
	if (!res)
	{
		this->error = CONF_VALUE_NOT_FOUND;
		return "";
	}
	return val;
}

bool ConfigReader::ReadFlag(std::string tag, std::string name, int index)
{
	char val[MAXBUF];
	char t[MAXBUF];
	char n[MAXBUF];
	strlcpy(t,tag.c_str(),MAXBUF);
	strlcpy(n,name.c_str(),MAXBUF);
	int res = Config->ReadConf(cache,t,n,index,val);
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
	strlcpy(t,tag.c_str(),MAXBUF);
	strlcpy(n,name.c_str(),MAXBUF);
	int res = Config->ReadConf(cache,t,n,index,val);
	if (!res)
	{
		this->error = CONF_VALUE_NOT_FOUND;
		return 0;
	}
	for (unsigned int i = 0; i < strlen(val); i++)
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

void ConfigReader::DumpErrors(bool bail, userrec* user)
{
        if (bail)
        {
                printf("There were errors in your configuration:\n%s",errorlog->str().c_str());
                exit(0);
        }
        else
        {
                char dataline[1024];
                if (user)
                {
                        WriteServ(user->fd,"NOTICE %s :There were errors in the configuration file:",user->nick);
                        while (!errorlog->eof())
                        {
                                errorlog->getline(dataline,1024);
                                WriteServ(user->fd,"NOTICE %s :%s",user->nick,dataline);
                        }
                }
                else
                {
                        WriteOpers("There were errors in the configuration file:",user->nick);
                        while (!errorlog->eof())
                        {
                                errorlog->getline(dataline,1024);
                                WriteOpers(dataline);
                        }
                }
                return;
        }
}


int ConfigReader::Enumerate(std::string tag)
{
	return Config->EnumConf(cache,tag.c_str());
}

int ConfigReader::EnumerateValues(std::string tag, int index)
{
	return Config->EnumValues(cache, tag.c_str(), index);
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
	if ((x<0) || ((unsigned)x>fc.size()))
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


