/*


*/



#include <typeinfo>
#include <iostream.h>
#include "globals.h"
#include "modules.h"
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
	void SetInfo(char mc, int ty, bool d_on, int p_on, int p_off) : modechar(mc), type(ty), default_on(d_on), params_when_on(p_on), params_when_off(p_off) { };
};                                     

typedef vector<ExtMode> ExtModeList;
typedef ExtModeList::iterator ExtModeListIter;

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
bool AddExtendedMode(char modechar, int type, bool default_on, int params_on, int params_off)
{
	ExtMode Mode;
	Mode.SetInfo(modechar,type,default_on,params_on,params_off);
	EMode.push_back(Mode);
	return true;
}


// version is a simple class for holding a modules version number

Version::Version(int major, int minor, int revision, int build) : Major(major), Minor(minor), Revision(revision), Build(build) { };

// admin is a simple class for holding a server's administrative info

Admin::Admin(string name, string email, string nick) : Name(name), Email(email), Nick(nick) { };

//
// Announce to the world that the Module base
// class has been created or destroyed
//

Module::Module() { }
Module::~Module() { }
void Module::OnUserConnect(userrec* user) { }
void Module::OnUserQuit(userrec* user) { }
void Module::OnUserJoin(userrec* user, chanrec* channel) { }
void Module::OnUserPart(userrec* user, chanrec* channel) { }
void Module::OnPacketTransmit(char *p) { }
void Module::OnPacketReceive(char *p) { }
void Module::OnRehash() { }
void Module::OnServerRaw(string &raw, bool inbound) { }
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

void Server::SendOpers(string s)
{
	WriteOpers("%s",s.c_str());
}

void Server::Log(int level, string s)
{
	log(level,"%s",s.c_str());
}

void Server::Send(int Socket, string s)
{
	Write(Socket,"%s",s.c_str());
}

void Server::SendServ(int Socket, string s)
{
	WriteServ(Socket,"%s",s.c_str());
}

void Server::SendFrom(int Socket, userrec* User, string s)
{
	WriteFrom(Socket,User,"%s",s.c_str());
}

void Server::SendTo(userrec* Source, userrec* Dest, string s)
{
	WriteTo(Source,Dest,"%s",s.c_str());
}

void Server::SendChannel(userrec* User, chanrec* Channel, string s,bool IncludeSender)
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

void Server::SendCommon(userrec* User, string text,bool IncludeSender)
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

void Server::SendWallops(userrec* User, string text)
{
	WriteWallOps(User,"%s",text.c_str());
}

bool Server::IsNick(string nick)
{
	return (isnick(nick.c_str()) != 0);
}

userrec* Server::FindNick(string nick)
{
	return Find(nick);
}

chanrec* Server::FindChannel(string channel)
{
	return FindChan(channel.c_str());
}

string Server::ChanMode(userrec* User, chanrec* Chan)
{
	string mode = cmode(User,Chan);
	return mode;
}

string Server::GetServerName()
{
	return getservername();
}

string Server::GetNetworkName()
{
	return getnetworkname();
}

Admin Server::GetAdmin()
{
	return Admin(getadminname(),getadminemail(),getadminnick());
}



bool Server::AddExtendedMode(char modechar, int type, bool default_on, int params_when_on, int params_when_off)
{
}


ConfigReader::ConfigReader()
{
	fname = CONFIG_FILE;
}


ConfigReader::~ConfigReader()
{
}


ConfigReader::ConfigReader(string filename) : fname(filename) { };

string ConfigReader::ReadValue(string tag, string name, int index)
{
	char val[MAXBUF];
	ReadConf(fname.c_str(),tag.c_str(),name.c_str(),index,val);
	string s = val;
	return s;
}


int ConfigReader::Enumerate(string tag)
{
	return EnumConf(fname.c_str(),tag.c_str());
}


bool ConfigReader::Verify()
{
	return true;
}


FileReader::FileReader(string filename)
{
	file_cache c;
	readfile(c,filename.c_str());
	this->fc = c;
}

FileReader::FileReader()
{
}

void FileReader::LoadFile(string filename)
{
	file_cache c;
	readfile(c,filename.c_str());
	this->fc = c;
}

FileReader::~FileReader()
{
}

string FileReader::GetLine(int x)
{
	if ((x<0) || (x>fc.size()))
		return "";
	return fc[x];
}

int FileReader::FileSize()
{
	return fc.size();
}


vector<Module*> modules(255);
vector<ircd_module*> factory(255);

int MODCOUNT  = -1;


