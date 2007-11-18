/*	   +------------------------------------+
 *	   | Inspire Internet Relay Chat Daemon |
 *	   +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *			the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Remote includes HTTP scheme */

using irc::sockets::OpenTCPSocket;

class ModuleRemoteIncludeHttp : public Module
{
 public:

	ModuleRemoteIncludeHttp(InspIRCd* Me)
		: Module(Me)
	{
		// The constructor just makes a copy of the server class
	
		
		Implementation eventlist[] = { I_OnDownloadFile };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}

	virtual ~ModuleRemoteIncludeHttp()
	{
	}

	virtual int OnDownloadFile(const std::string &filename, std::istream* &filedata)
	{
		std::stringstream* gotfile = (std::stringstream*)filedata;
		ServerInstance->Log(DEBUG,"OnDownloadFile in m_remoteinclude_http");
		int sockfd, portno, n;
		struct sockaddr_in serv_addr;
		struct hostent *server;
		char buffer[65536];

		/* Ours? */
		if (filename.substr(0, 7) != "http://")
			return 0;

		portno = 80;
		server = gethostbyname("neuron.brainbox.winbot.co.uk");

		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0)
		{
			ServerInstance->Log(DEBUG,"Failed to socket()");
			return 0;
		}

		if (server == NULL) 
		{
			ServerInstance->Log(DEBUG,"No such host");
			return 0;
		}

		memset(&serv_addr, sizeof(serv_addr), 0);

		serv_addr.sin_family = AF_INET;

		memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
		serv_addr.sin_port = htons(portno);

		if (connect(sockfd, (const sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) 
		{
			ServerInstance->Log(DEBUG,"Failed to connect()");
			return 0;
		}

		ServerInstance->Log(DEBUG,"Connected to brainbox");

		n = this->SockSend(sockfd, "GET / HTTP/1.1\r\nConnection: close\r\nHost: neuron.brainbox.winbot.co.uk\r\n\r\n");
		if (n < 0) 
		{
			ServerInstance->Log(DEBUG,"Failed to send()");
			return 0;
		}

		ServerInstance->Log(DEBUG,"Sent GET request");

		while (((n = read(sockfd,buffer,65535)) > 0))
		{
			std::string output(buffer, 0, n);
			(*(gotfile)) << output;
		}

		ServerInstance->Log(DEBUG,"Read page");

		std::string version, result;
		(*(gotfile)) >> version;
		(*(gotfile)) >> result;

		/* HTTP/1.1 200 OK */

		ServerInstance->Log(DEBUG,"Result: %s", result.c_str());

		return (result == "200");
	}

	int SockSend(int sock, const std::string &data)
	{
		return send(sock, data.data(), data.length(), 0);
	}

	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
};


MODULE_INIT(ModuleRemoteIncludeHttp)

