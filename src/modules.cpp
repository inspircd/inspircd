/*


*/



#include <typeinfo>
#include <iostream>
#include "globals.h"
#include "modules.h"
#include "ctables.h"
#include "inspircd_io.h"

// class type for holding an extended mode character - internal to core

class ExtMode
{
public:
	char modechar;
	int type;
	bool default_on;
	int params_when_on;
	int params_when_off;
	ExtMode(char mc, int ty, bool d_on, int p_on, int p_off) : modechar(mc), type(ty), default_on(d_on), params_when_on(p_on), params_when_off(p_off) { };
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
bool DoAddExtendedMode(char modechar, int type, bool default_on, int params_on, int params_off)
{
	if (ModeDefined(modechar,type)) {
		return false;
	}
	EMode.push_back(ExtMode(modechar,type,default_on,params_on,params_off));
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
int Module::OnUserPreJoin(userrec* user, chanrec* chan, char* cname) { return 0; }
bool Module::OnExtendedMode(userrec* user, chanrec* chan, char modechar, int type, bool mode_on, string_list &params) { }
Version Module::GetVersion() { return Version(1,0,0,0); }

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
	WriteTo(Source,Dest,"%s",s.c_str());
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



bool Server::AddExtendedMode(char modechar, int type, bool default_on, int params_when_on, int params_when_off)
{
	return DoAddExtendedMode(modechar,type,default_on,params_when_on,params_when_off);
}


ConfigReader::ConfigReader()
{
	fname = CONFIG_FILE;
}


ConfigReader::~ConfigReader()
{
}


ConfigReader::ConfigReader(std::string filename) : fname(filename) { };

std::string ConfigReader::ReadValue(std::string tag, std::string name, int index)
{
	char val[MAXBUF];
	ReadConf(fname.c_str(),tag.c_str(),name.c_str(),index,val);
	return val;
}


int ConfigReader::Enumerate(std::string tag)
{
	return EnumConf(fname.c_str(),tag.c_str());
}


bool ConfigReader::Verify()
{
	return true;
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


