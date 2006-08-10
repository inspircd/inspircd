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

#include "inspircd_config.h"
//#include "inspircd.h"
#include "configreader.h"
#include <unistd.h>
#include <sys/errno.h>
#include <time.h>
#include <string>
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
#include "dns.h"
#include "inspircd.h"

extern InspIRCd* ServerInstance;
extern time_t TIME;
extern command_table cmdlist;

class Server;

// version is a simple class for holding a modules version number

Version::Version(int major, int minor, int revision, int build, int flags)
: Major(major), Minor(minor), Revision(revision), Build(build), Flags(flags)
{
}

Request::Request(char* anydata, Module* src, Module* dst)
: data(anydata), source(src), dest(dst)
{
	/* Ensure that because this module doesnt support ID strings, it doesnt break modules that do
	 * by passing them uninitialized pointers (could happen)
	 */
	id = '\0';
}

Request::Request(Module* src, Module* dst, const char* idstr)
: id(idstr), source(src), dest(dst)
{
};

char* Request::GetData()
{
	return this->data;
}

const char* Request::GetId()
{
	return this->id;
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

Event::Event(char* anydata, Module* src, const std::string &eventid) : data(anydata), source(src), id(eventid) { };

char* Event::GetData()
{
	return (char*)this->data;
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
void		Module::OnUserQuit(userrec* user, const std::string& message) { }
void		Module::OnUserDisconnect(userrec* user) { }
void		Module::OnUserJoin(userrec* user, chanrec* channel) { }
void		Module::OnUserPart(userrec* user, chanrec* channel, const std::string &partmessage) { }
void		Module::OnRehash(const std::string &parameter) { }
void		Module::OnServerRaw(std::string &raw, bool inbound, userrec* user) { }
int		Module::OnUserPreJoin(userrec* user, chanrec* chan, const char* cname) { return 0; }
void		Module::OnMode(userrec* user, void* dest, int target_type, const std::string &text) { };
Version		Module::GetVersion() { return Version(1,0,0,0,VF_VENDOR); }
void		Module::OnOper(userrec* user, const std::string &opertype) { };
void		Module::OnPostOper(userrec* user, const std::string &opertype) { };
void		Module::OnInfo(userrec* user) { };
void		Module::OnWhois(userrec* source, userrec* dest) { };
int		Module::OnUserPreInvite(userrec* source,userrec* dest,chanrec* channel) { return 0; };
int		Module::OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text,char status) { return 0; };
int		Module::OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text,char status) { return 0; };
int		Module::OnUserPreNick(userrec* user, const std::string &newnick) { return 0; };
void		Module::OnUserPostNick(userrec* user, const std::string &oldnick) { };
int		Module::OnAccessCheck(userrec* source,userrec* dest,chanrec* channel,int access_type) { return ACR_DEFAULT; };
void		Module::On005Numeric(std::string &output) { };
int		Module::OnKill(userrec* source, userrec* dest, const std::string &reason) { return 0; };
void		Module::OnLoadModule(Module* mod,const std::string &name) { };
void		Module::OnUnloadModule(Module* mod,const std::string &name) { };
void		Module::OnBackgroundTimer(time_t curtime) { };
int		Module::OnPreCommand(const std::string &command, const char** parameters, int pcnt, userrec *user, bool validated) { return 0; };
bool		Module::OnCheckReady(userrec* user) { return true; };
void		Module::OnUserRegister(userrec* user) { };
int		Module::OnUserPreKick(userrec* source, userrec* user, chanrec* chan, const std::string &reason) { return 0; };
void		Module::OnUserKick(userrec* source, userrec* user, chanrec* chan, const std::string &reason) { };
int		Module::OnRawMode(userrec* user, chanrec* chan, char mode, const std::string &param, bool adding, int pcnt) { return 0; };
int		Module::OnCheckInvite(userrec* user, chanrec* chan) { return 0; };
int		Module::OnCheckKey(userrec* user, chanrec* chan, const std::string &keygiven) { return 0; };
int		Module::OnCheckLimit(userrec* user, chanrec* chan) { return 0; };
int		Module::OnCheckBan(userrec* user, chanrec* chan) { return 0; };
int		Module::OnStats(char symbol, userrec* user, string_list &results) { return 0; };
int		Module::OnChangeLocalUserHost(userrec* user, const std::string &newhost) { return 0; };
int		Module::OnChangeLocalUserGECOS(userrec* user, const std::string &newhost) { return 0; };
int		Module::OnLocalTopicChange(userrec* user, chanrec* chan, const std::string &topic) { return 0; };
void		Module::OnEvent(Event* event) { return; };
char*		Module::OnRequest(Request* request) { return NULL; };
int		Module::OnOperCompare(const std::string &password, const std::string &input) { return 0; };
void		Module::OnGlobalOper(userrec* user) { };
void		Module::OnGlobalConnect(userrec* user) { };
int		Module::OnAddBan(userrec* source, chanrec* channel,const std::string &banmask) { return 0; };
int		Module::OnDelBan(userrec* source, chanrec* channel,const std::string &banmask) { return 0; };
void		Module::OnRawSocketAccept(int fd, const std::string &ip, int localport) { };
int		Module::OnRawSocketWrite(int fd, const char* buffer, int count) { return 0; };
void		Module::OnRawSocketClose(int fd) { };
int		Module::OnRawSocketRead(int fd, char* buffer, unsigned int count, int &readresult) { return 0; };
void		Module::OnUserMessage(userrec* user, void* dest, int target_type, const std::string &text, char status) { };
void		Module::OnUserNotice(userrec* user, void* dest, int target_type, const std::string &text, char status) { };
void 		Module::OnRemoteKill(userrec* source, userrec* dest, const std::string &reason) { };
void		Module::OnUserInvite(userrec* source,userrec* dest,chanrec* channel) { };
void		Module::OnPostLocalTopicChange(userrec* user, chanrec* chan, const std::string &topic) { };
void		Module::OnGetServerDescription(const std::string &servername,std::string &description) { };
void		Module::OnSyncUser(userrec* user, Module* proto, void* opaque) { };
void		Module::OnSyncChannel(chanrec* chan, Module* proto, void* opaque) { };
void		Module::ProtoSendMode(void* opaque, int target_type, void* target, const std::string &modeline) { };
void		Module::OnSyncChannelMetaData(chanrec* chan, Module* proto,void* opaque, const std::string &extname) { };
void		Module::OnSyncUserMetaData(userrec* user, Module* proto,void* opaque, const std::string &extname) { };
void		Module::OnSyncOtherMetaData(Module* proto, void* opaque) { };
void		Module::OnDecodeMetaData(int target_type, void* target, const std::string &extname, const std::string &extdata) { };
void		Module::ProtoSendMetaData(void* opaque, int target_type, void* target, const std::string &extname, const std::string &extdata) { };
void		Module::OnWallops(userrec* user, const std::string &text) { };
void		Module::OnChangeHost(userrec* user, const std::string &newhost) { };
void		Module::OnChangeName(userrec* user, const std::string &gecos) { };
void		Module::OnAddGLine(long duration, userrec* source, const std::string &reason, const std::string &hostmask) { };
void		Module::OnAddZLine(long duration, userrec* source, const std::string &reason, const std::string &ipmask) { };
void		Module::OnAddKLine(long duration, userrec* source, const std::string &reason, const std::string &hostmask) { };
void		Module::OnAddQLine(long duration, userrec* source, const std::string &reason, const std::string &nickmask) { };
void		Module::OnAddELine(long duration, userrec* source, const std::string &reason, const std::string &hostmask) { };
void		Module::OnDelGLine(userrec* source, const std::string &hostmask) { };
void		Module::OnDelZLine(userrec* source, const std::string &ipmask) { };
void		Module::OnDelKLine(userrec* source, const std::string &hostmask) { };
void		Module::OnDelQLine(userrec* source, const std::string &nickmask) { };
void		Module::OnDelELine(userrec* source, const std::string &hostmask) { };
void 		Module::OnCleanup(int target_type, void* item) { };
void		Module::Implements(char* Implements) { for (int j = 0; j < 255; j++) Implements[j] = 0; };
void		Module::OnChannelDelete(chanrec* chan) { };
Priority	Module::Prioritize() { return PRIORITY_DONTCARE; }
void		Module::OnSetAway(userrec* user) { };
void		Module::OnCancelAway(userrec* user) { };

/* server is a wrapper class that provides methods to all of the C-style
 * exports in the core
 */

void Server::AddSocket(InspSocket* sock)
{
	ServerInstance->module_sockets.push_back(sock);
}

void Server::RemoveSocket(InspSocket* sock)
{
	for (std::vector<InspSocket*>::iterator a = ServerInstance->module_sockets.begin(); a < ServerInstance->module_sockets.end(); a++)
	{
		InspSocket* s = (InspSocket*)*a;
		if (s == sock)
			s->MarkAsClosed();
	}
}

long InspIRCd::PriorityAfter(const std::string &modulename)
{
	for (unsigned int j = 0; j < this->Config->module_names.size(); j++)
	{
		if (this->Config->module_names[j] == modulename)
		{
			return ((j << 8) | PRIORITY_AFTER);
		}
	}
	return PRIORITY_DONTCARE;
}

long InspIRCd::PriorityBefore(const std::string &modulename)
{
	for (unsigned int j = 0; j < this->Config->module_names.size(); j++)
	{
		if (this->Config->module_names[j] == modulename)
		{
			return ((j << 8) | PRIORITY_BEFORE);
		}
	}
	return PRIORITY_DONTCARE;
}

bool InspIRCd::PublishFeature(const std::string &FeatureName, Module* Mod)
{
	if (Features.find(FeatureName) == Features.end())
	{
		Features[FeatureName] = Mod;
		return true;
	}
	return false;
}

bool InspIRCd::UnpublishFeature(const std::string &FeatureName)
{
	featurelist::iterator iter = Features.find(FeatureName);
	
	if (iter == Features.end())
		return false;

	Features.erase(iter);
	return true;
}

Module* InspIRCd::FindFeature(const std::string &FeatureName)
{
	featurelist::iterator iter = Features.find(FeatureName);

	if (iter == Features.end())
		return NULL;

	return iter->second;
}

const std::string& InspIRCd::GetModuleName(Module* m)
{
	static std::string nothing = ""; /* Prevent compiler warning */
	for (int i = 0; i <= this->GetModuleCount(); i++)
	{
		if (this->modules[i] == m)
		{
			return this->Config->module_names[i];
		}
	}
	return nothing; /* As above */
}

void Server::RehashServer()
{
	ServerInstance->WriteOpers("*** Rehashing config file");
	ServerInstance->Config->Read(false,NULL);
}

void Server::DelSocket(InspSocket* sock)
{
	for (std::vector<InspSocket*>::iterator a = ServerInstance->module_sockets.begin(); a < ServerInstance->module_sockets.end(); a++)
	{
		if (*a == sock)
		{
			ServerInstance->module_sockets.erase(a);
			return;
		}
	}
}

/* This is ugly, yes, but hash_map's arent designed to be
 * addressed in this manner, and this is a bit of a kludge.
 * Luckily its a specialist function and rarely used by
 * many modules (in fact, it was specially created to make
 * m_safelist possible, initially).
 */

chanrec* Server::GetChannelIndex(long index)
{
	int target = 0;
	for (chan_hash::iterator n = ServerInstance->chanlist.begin(); n != ServerInstance->chanlist.end(); n++, target++)
	{
		if (index == target)
			return n->second;
	}
	return NULL;
}

bool Server::MatchText(const std::string &sliteral, const std::string &spattern)
{
	return match(sliteral.c_str(),spattern.c_str());
}

bool Server::IsUlined(const std::string &server)
{
	return is_uline(server.c_str());
}

bool Server::CallCommandHandler(const std::string &commandname, const char** parameters, int pcnt, userrec* user)
{
	return ServerInstance->Parser->CallHandler(commandname,parameters,pcnt,user);
}

bool Server::IsValidModuleCommand(const std::string &commandname, int pcnt, userrec* user)
{
	return ServerInstance->Parser->IsValidCommand(commandname, pcnt, user);
}

void Server::AddCommand(command_t *f)
{
	if (!ServerInstance->Parser->CreateCommand(f))
	{
		ModuleException err("Command "+std::string(f->command)+" already exists.");
		throw (err);
	}
}

void Server::SendMode(const char** parameters, int pcnt, userrec *user)
{
	ServerInstance->ModeGrok->Process(parameters,pcnt,user,true);
}

void Server::DumpText(userrec* User, const std::string &LinePrefix, stringstream &TextStream)
{
	std::string CompleteLine = LinePrefix;
	std::string Word = "";
	while (TextStream >> Word)
	{
		if (CompleteLine.length() + Word.length() + 3 > 500)
		{
			User->WriteServ(CompleteLine);
			CompleteLine = LinePrefix;
		}
		CompleteLine = CompleteLine + Word + " ";
	}
	User->WriteServ(CompleteLine);
}

userrec* Server::FindDescriptor(int socket)
{
	return (socket < 65536 ? ServerInstance->fd_ref_table[socket] : NULL);
}

bool Server::AddMode(ModeHandler* mh, const unsigned char mode)
{
	return ServerInstance->ModeGrok->AddMode(mh,mode);
}

bool Server::AddModeWatcher(ModeWatcher* mw)
{
	return ServerInstance->ModeGrok->AddModeWatcher(mw);
}

bool Server::DelModeWatcher(ModeWatcher* mw)
{
	return ServerInstance->ModeGrok->DelModeWatcher(mw);
}

bool Server::AddResolver(Resolver* r)
{
	return ServerInstance->Res->AddResolverClass(r);
}

bool InspIRCd::UserToPseudo(userrec* user, const std::string &message)
{
	unsigned int old_fd = user->fd;
	user->Write("ERROR :Closing link (%s@%s) [%s]",user->ident,user->host,message.c_str());
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

bool InspIRCd::PseudoToUser(userrec* alive, userrec* zombie, const std::string &message)
{
	log(DEBUG,"PseudoToUser");
	zombie->fd = alive->fd;
	FOREACH_MOD(I_OnUserQuit,OnUserQuit(alive,message));
	alive->fd = FD_MAGIC_NUMBER;
	alive->FlushWriteBuf();
	alive->ClearBuffer();
	// save these for later
	std::string oldnick = alive->nick;
	std::string oldhost = alive->host;
	std::string oldident = alive->ident;
	userrec::QuitUser(this,alive,message.c_str());
	if (find(local_users.begin(),local_users.end(),alive) != local_users.end())
	{
		local_users.erase(find(local_users.begin(),local_users.end(),alive));
		log(DEBUG,"Delete local user");
	}
	// Fix by brain - cant write the user until their fd table entry is updated
	ServerInstance->fd_ref_table[zombie->fd] = zombie;
	zombie->Write(":%s!%s@%s NICK %s",oldnick.c_str(),oldident.c_str(),oldhost.c_str(),zombie->nick);
	for (std::vector<ucrec*>::const_iterator i = zombie->chans.begin(); i != zombie->chans.end(); i++)
	{
		if (((ucrec*)(*i))->channel != NULL)
		{
				chanrec* Ptr = ((ucrec*)(*i))->channel;
				zombie->WriteFrom(zombie,"JOIN %s",Ptr->name);
				if (Ptr->topicset)
				{
					zombie->WriteServ("332 %s %s :%s", zombie->nick, Ptr->name, Ptr->topic);
					zombie->WriteServ("333 %s %s %s %d", zombie->nick, Ptr->name, Ptr->setby, Ptr->topicset);
				}
				Ptr->UserList(zombie);
				zombie->WriteServ("366 %s %s :End of /NAMES list.", zombie->nick, Ptr->name);
		}
	}
	if ((find(local_users.begin(),local_users.end(),zombie) == local_users.end()) && (zombie->fd != FD_MAGIC_NUMBER))
		local_users.push_back(zombie);

	return true;
}

void Server::AddGLine(long duration, const std::string &source, const std::string &reason, const std::string &hostmask)
{
	add_gline(duration, source.c_str(), reason.c_str(), hostmask.c_str());
	apply_lines(APPLY_GLINES);
}

void Server::AddQLine(long duration, const std::string &source, const std::string &reason, const std::string &nickname)
{
	add_qline(duration, source.c_str(), reason.c_str(), nickname.c_str());
	apply_lines(APPLY_QLINES);
}

void Server::AddZLine(long duration, const std::string &source, const std::string &reason, const std::string &ipaddr)
{
	add_zline(duration, source.c_str(), reason.c_str(), ipaddr.c_str());
	apply_lines(APPLY_ZLINES);
}

void Server::AddKLine(long duration, const std::string &source, const std::string &reason, const std::string &hostmask)
{
	add_kline(duration, source.c_str(), reason.c_str(), hostmask.c_str());
	apply_lines(APPLY_KLINES);
}

void Server::AddELine(long duration, const std::string &source, const std::string &reason, const std::string &hostmask)
{
	add_eline(duration, source.c_str(), reason.c_str(), hostmask.c_str());
}

bool Server::DelGLine(const std::string &hostmask)
{
	return del_gline(hostmask.c_str());
}

bool Server::DelQLine(const std::string &nickname)
{
	return del_qline(nickname.c_str());
}

bool Server::DelZLine(const std::string &ipaddr)
{
	return del_zline(ipaddr.c_str());
}

bool Server::DelKLine(const std::string &hostmask)
{
	return del_kline(hostmask.c_str());
}

bool Server::DelELine(const std::string &hostmask)
{
	return del_eline(hostmask.c_str());
}

long Server::CalcDuration(const std::string &delta)
{
	return duration(delta.c_str());
}

/*
 * XXX why on *earth* is this in modules.cpp...? I think
 * perhaps we need a server.cpp for Server:: stuff where possible. -- w00t
 */
bool Server::IsValidMask(const std::string &mask)
{
	char* dest = (char*)mask.c_str();
	if (strchr(dest,'!')==0)
		return false;
	if (strchr(dest,'@')==0)
		return false;
	for (char* i = dest; *i; i++)
		if (*i < 32)
			return false;
	for (char* i = dest; *i; i++)
		if (*i > 126)
			return false;
	unsigned int c = 0;
	for (char* i = dest; *i; i++)
		if (*i == '!')
			c++;
	if (c>1)
		return false;
	c = 0;
	for (char* i = dest; *i; i++)
		if (*i == '@')
			c++;
	if (c>1)
		return false;

	return true;
}

Module* InspIRCd::FindModule(const std::string &name)
{
	for (int i = 0; i <= this->GetModuleCount(); i++)
	{
		if (this->Config->module_names[i] == name)
		{
			return this->modules[i];
		}
	}
	return NULL;
}

ConfigReader::ConfigReader()
{
	// ServerInstance->Config->ClearStack();
	
	/* Is there any reason to load the entire config file again here?
	 * it's needed if they specify another config file, but using the
	 * default one we can just use the global config data - pre-parsed!
	 */
	//~ this->cache = new std::stringstream(std::stringstream::in | std::stringstream::out);
	this->errorlog = new std::ostringstream(std::stringstream::in | std::stringstream::out);
	
	//~ this->readerror = ServerInstance->Config->LoadConf(CONFIG_FILE, this->cache,this->errorlog);
	//~ if (!this->readerror)
		//~ this->error = CONF_FILE_NOT_FOUND;
	
	this->data = &ServerInstance->Config->config_data;
	this->privatehash = false;
}


ConfigReader::~ConfigReader()
{
	//~ if (this->cache)
		//~ delete this->cache;
	if (this->errorlog)
		DELETE(this->errorlog);
	if(this->privatehash)
		DELETE(this->data);
}


ConfigReader::ConfigReader(const std::string &filename)
{
	ServerInstance->Config->ClearStack();
	
	this->data = new ConfigDataHash;
	this->privatehash = true;
	this->errorlog = new std::ostringstream(std::stringstream::in | std::stringstream::out);
	this->readerror = ServerInstance->Config->LoadConf(*this->data, filename, *this->errorlog);
	if (!this->readerror)
		this->error = CONF_FILE_NOT_FOUND;
};

std::string ConfigReader::ReadValue(const std::string &tag, const std::string &name, int index)
{
	/* Don't need to strlcpy() tag and name anymore, ReadConf() takes const char* */ 
	std::string result;
	
	if (!ServerInstance->Config->ConfValue(*this->data, tag, name, index, result))
	{
		this->error = CONF_VALUE_NOT_FOUND;
		return "";
	}
	
	return result;
}

bool ConfigReader::ReadFlag(const std::string &tag, const std::string &name, int index)
{
	return ServerInstance->Config->ConfValueBool(*this->data, tag, name, index);
}

long ConfigReader::ReadInteger(const std::string &tag, const std::string &name, int index, bool needs_unsigned)
{
	int result;
	
	if(!ServerInstance->Config->ConfValueInteger(*this->data, tag, name, index, result))
	{
		this->error = CONF_VALUE_NOT_FOUND;
		return 0;
	}
	
	if ((needs_unsigned) && (result < 0))
	{
		this->error = CONF_NOT_UNSIGNED;
		return 0;
	}
	
	return result;
}

long ConfigReader::GetError()
{
	long olderr = this->error;
	this->error = 0;
	return olderr;
}

void ConfigReader::DumpErrors(bool bail, userrec* user)
{
	/* XXX - Duplicated code */
	
	if (bail)
	{
		printf("There were errors in your configuration:\n%s", this->errorlog->str().c_str());
		InspIRCd::Exit(ERROR);
	}
	else
	{
		std::string errors = this->errorlog->str();
		std::string::size_type start;
		unsigned int prefixlen;
		
		start = 0;
		/* ":ServerInstance->Config->ServerName NOTICE user->nick :" */
		prefixlen = strlen(ServerInstance->Config->ServerName) + strlen(user->nick) + 11;
	
		if (user)
		{
			user->WriteServ("NOTICE %s :There were errors in the configuration file:",user->nick);
			
			while(start < errors.length())
			{
				user->WriteServ("NOTICE %s :%s",user->nick, errors.substr(start, 510 - prefixlen).c_str());
				start += 510 - prefixlen;
			}
		}
		else
		{
			ServerInstance->WriteOpers("There were errors in the configuration file:");
			
			while(start < errors.length())
			{
				ServerInstance->WriteOpers(errors.substr(start, 360).c_str());
				start += 360;
			}
		}

		return;
	}
}


int ConfigReader::Enumerate(const std::string &tag)
{
	return ServerInstance->Config->ConfValueEnum(*this->data, tag);
}

int ConfigReader::EnumerateValues(const std::string &tag, int index)
{
	return ServerInstance->Config->ConfVarEnum(*this->data, tag, index);
}

bool ConfigReader::Verify()
{
	return this->readerror;
}


FileReader::FileReader(const std::string &filename)
{
	file_cache c;
	ServerInstance->Config->ReadFile(c,filename.c_str());
	this->fc = c;
	this->CalcSize();
}

FileReader::FileReader()
{
}

std::string FileReader::Contents()
{
	std::string x = "";
	for (file_cache::iterator a = this->fc.begin(); a != this->fc.end(); a++)
	{
		x.append(*a);
		x.append("\r\n");
	}
	return x;
}

unsigned long FileReader::ContentSize()
{
	return this->contentsize;
}

void FileReader::CalcSize()
{
	unsigned long n = 0;
	for (file_cache::iterator a = this->fc.begin(); a != this->fc.end(); a++)
		n += (a->length() + 2);
	this->contentsize = n;
}

void FileReader::LoadFile(const std::string &filename)
{
	file_cache c;
	ServerInstance->Config->ReadFile(c,filename.c_str());
	this->fc = c;
	this->CalcSize();
}


FileReader::~FileReader()
{
}

bool FileReader::Exists()
{
	return (!(fc.size() == 0));
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


