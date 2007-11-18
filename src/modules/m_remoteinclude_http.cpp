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
		ServerInstance->Log(DEBUG,"OnDownloadFile in m_remoteinclude_http");
		int sockfd, portno, n;
		struct sockaddr_in serv_addr;
		struct hostent *server;
		char buffer[65536];

		portno = 80;
		server = gethostbyname("neuron.brainbox.winbot.co.uk");

		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0) 
			return 0;

		if (server == NULL) 
			return 0;

		memset(&serv_addr, sizeof(serv_addr), 0);

		serv_addr.sin_family = AF_INET;

		memcpy(server->h_addr, &serv_addr.sin_addr.s_addr, server->h_length);
		serv_addr.sin_port = htons(portno);

		if (connect(sockfd, (const sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) 
			return 0;

		n = send(sockfd, "GET / HTTP/1.0\r\n", 16, 0);
		if (n < 0) 
			return 0;

		n = read(sockfd,buffer,255);

		if (n < 1) 
			return 0;

		return 1;
	}

	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
};


MODULE_INIT(ModuleRemoteIncludeHttp)

