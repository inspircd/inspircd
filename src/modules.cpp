/*


*/



#include <typeinfo>
#include <iostream>
#include "globals.h"
#include "modules.h"
#include "ctables.h"
#include "inspircd_io.h"
#include "wildcard.h"

// class type for holding an extended mode character - internal to core

class ExtMode
{
public:
	char modechar;
	int type;
	int params_when_on;
	int params_when_off;
	bool needsoper;
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
		log(DEBUG,"i->modechar==%c, modechar=%c, i->type=%d, type=%d",i->modechar,modechar,i->type,type);
		if ((i->modechar == modechar) && (i->type == type))
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
		log(DEBUG,"i->modechar==%c, modechar=%c, i->type=%d, type=%d",i->modechar,modechar,i->type,type);
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
void Module::OnServerRaw(std::string &raw, bool inbound) { }
int Module::OnUserPreJoin(userrec* user, chanrec* chan, const char* cname) { return 0; }
bool Module::OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params) { return false; }
Version Module::GetVersion() { return Version(1,0,0,0); }
void Module::OnOper(userrec* user) { };
void Module::OnInfo(userrec* user) { };
void Module::OnWhois(userrec* source, userrec* dest) { };
int Module::OnUserPreMessage(userrec* user,void* dest,int target_type, std::string text) { return 0; };
int Module::OnUserPreNotice(userrec* user,void* dest,int target_type, std::string text) { return 0; };

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

void Server::ChangeUserNick(userrec* user, std::string nickname)
{
	force_nickchange(user,nickname.c_str());
}

void Server::QuitUser(userrec* user, std::string reason)
{
	send_network_quit(user->nick,reason.c_str());
	kill_link(user,reason.c_str());
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
	WriteWallOps(User,"%s",text.c_str());
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

int Server::CountUsers(chanrec* c)
{
	return usercount(c);
}


ConfigReader::ConfigReader()
{
	this->cache = new std::stringstream(std::stringstream::in | std::stringstream::out);
	this->error = LoadConf(CONFIG_FILE,this->cache);
}


ConfigReader::~ConfigReader()
{
	if (this->cache)
		delete this->cache;
}


ConfigReader::ConfigReader(std::string filename)
{
	this->cache = new std::stringstream(std::stringstream::in | std::stringstream::out);
	this->error = LoadConf(filename.c_str(),this->cache);
};

std::string ConfigReader::ReadValue(std::string tag, std::string name, int index)
{
	char val[MAXBUF];
	ReadConf(cache,tag.c_str(),name.c_str(),index,val);
	return val;
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
	return this->error;
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


