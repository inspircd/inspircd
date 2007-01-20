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

/* Written by Special (john@yarbbles.com) */

#include "inspircd.h"
#include "httpclient.h"

/* $ModDesc: HTTP client service provider */

class URL
{
 public:
	std::string url;
	std::string protocol, username, password, domain, request;
	int port;
};

class HTTPSocket : public InspSocket
{
 private:
	InspIRCd *Server;
	class ModuleHTTPClient *Mod;
	HTTPClientRequest req;
	HTTPClientResponse *response;
	URL url;
	enum { HTTP_CLOSED, HTTP_REQSENT, HTTP_HEADERS, HTTP_DATA } status;
	std::string data;
	std::string buffer;

 public:
	HTTPSocket(InspIRCd *Instance, class ModuleHTTPClient *Mod);
	virtual ~HTTPSocket();
	virtual bool DoRequest(HTTPClientRequest *req);
	virtual bool ParseURL(const std::string &url);
	virtual void Connect(const std::string &ip);
	virtual bool OnConnected();
	virtual bool OnDataReady();
	virtual void OnClose();
};

class HTTPResolver : public Resolver
{
 private:
	HTTPSocket *socket;
 public:
	HTTPResolver(HTTPSocket *socket, InspIRCd *Instance, const string &hostname, bool &cached, Module* me) : Resolver(Instance, hostname, DNS_QUERY_FORWARD, cached, me), socket(socket)
	{
	}
	
	void OnLookupComplete(const string &result, unsigned int ttl, bool cached)
	{
		socket->Connect(result);
	}
	
	void OnError(ResolverError e, const string &errmsg)
	{
		delete socket;
	}
};

typedef vector<HTTPSocket*> HTTPList;

class ModuleHTTPClient : public Module
{
 public:
	HTTPList sockets;

	ModuleHTTPClient(InspIRCd *Me)
		: Module::Module(Me)
	{
	}
	
	virtual ~ModuleHTTPClient()
	{
		for (HTTPList::iterator i = sockets.begin(); i != sockets.end(); i++)
			delete *i;
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_SERVICEPROVIDER | VF_VENDOR, API_VERSION);
	}

	void Implements(char* List)
	{
		List[I_OnRequest] = 1;
	}

	char* OnRequest(Request *req)
	{
		HTTPClientRequest *httpreq = (HTTPClientRequest *)req;
		if (!strcmp(httpreq->GetId(), HTTP_CLIENT_REQUEST))
		{
			HTTPSocket *sock = new HTTPSocket(ServerInstance, this);
			sock->DoRequest(httpreq);
			// No return value
		}
		return NULL;
	}
};

HTTPSocket::HTTPSocket(InspIRCd *Instance, ModuleHTTPClient *Mod)
		: InspSocket(Instance), Server(Instance), Mod(Mod), status(HTTP_CLOSED)
{
	this->ClosePending = false;
	this->port = 80;
}

HTTPSocket::~HTTPSocket()
{
	Close();
	for (HTTPList::iterator i = Mod->sockets.begin(); i != Mod->sockets.end(); i++)
	{
		if (*i == this)
		{
			Mod->sockets.erase(i);
			break;
		}
	}
}

bool HTTPSocket::DoRequest(HTTPClientRequest *req)
{
	/* Tweak by brain - we take a copy of this,
	 * so that the caller doesnt need to leave
	 * pointers knocking around, less chance of
	 * a memory leak.
	 */
	this->req = *req;

	if (!ParseURL(this->req.GetURL()))
		return false;
	
	this->port = url.port;
	strlcpy(this->host, url.domain.c_str(), MAXBUF);

	if (!insp_aton(this->host, &this->addy))
	{
		bool cached;
		HTTPResolver* r = new HTTPResolver(this, Server, url.domain, cached, (Module*)Mod);
		Instance->AddResolver(r, cached);
		return true;
	}
	else
	{
		this->Connect(url.domain);
	}
	
	return true;
}

bool HTTPSocket::ParseURL(const std::string &iurl)
{
	url.url = iurl;
	url.port = 80;
	
	// Tokenize by slashes (protocol:, blank, domain, request..)
	int pos = 0, pstart = 0, pend = 0;
	
	for (; ; pend = url.url.find('/', pstart))
	{
		string part = url.url.substr(pstart, pend);

		switch (pos)
		{
			case 0:
				// Protocol
				if (part[part.length()-1] != ':')
					return false;
				url.protocol = part.substr(0, part.length() - 1);
				break;
			case 1:
				// Empty, skip
				break;
			case 2:
				// User and password (user:pass@)
				string::size_type aend = part.find('@', 0);
				if (aend != string::npos)
				{
					// Technically, it is valid to not have a password (username@domain)
					string::size_type usrend = part.find(':', 0);
					
					if ((usrend != string::npos) && (usrend < aend))
						url.password = part.substr(usrend + 1, aend);
					else
						usrend = aend;
					
					url.username = part.substr(0, usrend);
				}
				else
					aend = 0;
				
				// Port (:port)
				string::size_type dend = part.find(':', aend);
				if (dend != string::npos)
					url.port = atoi(part.substr(dend + 1).c_str());

				// Domain
				url.domain = part.substr(aend + 1, dend);
				
				// The rest of the string is the request
				url.request = url.url.substr(pend);
				break;
		}
		
		if (pos++ == 2)
			break;

		pstart = pend + 1;
	}
	
	return true;
}

void HTTPSocket::Connect(const string &ip)
{
	strlcpy(this->IP, ip.c_str(), MAXBUF);
	
	if (!this->DoConnect())
	{
		delete this;
	}
}

bool HTTPSocket::OnConnected()
{
	std::string request = "GET " + url.request + " HTTP/1.1\r\n";

	// Dump headers into the request
	HeaderMap headers = req.GetHeaders();
	
	for (HeaderMap::iterator i = headers.begin(); i != headers.end(); i++)
		request += i->first + ": " + i->second + "\r\n";

	// The Host header is required for HTTP 1.1 and isn't known when the request is created; if they didn't overload it
	// manually, add it here
	if (headers.find("Host") == headers.end())
		request += "Host: " + url.domain + "\r\n"; 
	
	request += "\r\n";
	
	this->status = HTTP_REQSENT;
	
	return this->Write(request);
}

bool HTTPSocket::OnDataReady()
{
	char *data = this->Read();

	if (!data)
	{
		this->Close();
		return false;
	}

	if (this->status < HTTP_DATA)
	{
		std::string line;
		std::string::size_type pos;

		this->buffer += data;
		while ((pos = buffer.find("\r\n")) != std::string::npos)
		{
			line = buffer.substr(0, pos);
			buffer = buffer.substr(pos + 2);
			if (line.empty())
			{
				this->status = HTTP_DATA;
				this->data += this->buffer;
				this->buffer = "";
				break;
			}
//		while ((line = buffer.sstrstr(data, "\r\n")) != NULL)
//		{
//			if (strncmp(data, "\r\n", 2) == 0)
			
			if (this->status == HTTP_REQSENT)
			{
				// HTTP reply (HTTP/1.1 200 msg)
				char const* data = line.c_str();
				data += 9;
				response = new HTTPClientResponse((Module*)Mod, req.GetSource() , url.url, atoi(data), data + 4);
				this->status = HTTP_HEADERS;
				continue;
			}
			
			if ((pos = line.find(':')) != std::string::npos)
			{

//			char *hdata = strchr(data, ':');
			
//			if (!hdata)
//				continue;
			
//			*hdata = '\0';
			
//			response->AddHeader(data, hdata + 2);
				response->AddHeader(line.substr(0, pos), line.substr(pos + 1));
			
//			data = lend + 2;
			} else
				continue;
		}
	} else {
		this->data += data;
	}
	return true;
}

void HTTPSocket::OnClose()
{
	if (data.empty())
		return; // notification that request failed?

	response->data = data;
	response->Send();
	delete response;
}

class ModuleHTTPClientFactory : public ModuleFactory
{
 public:
	ModuleHTTPClientFactory()
	{
	}
	
	~ModuleHTTPClientFactory()
	{
	}
	
	Module *CreateModule(InspIRCd* Me)
	{
		return new ModuleHTTPClient(Me);
	}
};

extern "C" void *init_module(void)
{
	return new ModuleHTTPClientFactory;
}
