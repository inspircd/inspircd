/*

$Log$
Revision 1.1  2003/01/23 19:45:58  brain
Initial revision

Revision 1.7  2003/01/22 20:49:16  brain
Added FileReader file-caching class
Changed m_randquote to use FileReader class

Revision 1.6  2003/01/21 20:31:24  brain
Modified to add documentation
Added ConfigReader class for modules

Revision 1.5  2003/01/13 22:30:50  brain
Added Admin class (holds /admin info for modules)
Added methods to Server class


*/



#include <typeinfo>
#include <iostream.h>
#include "globals.h"
#include "modules.h"
#include "inspircd_io.h"

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

void Server::Debug(string s)
{
	debug("%s",s.c_str());
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


