/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

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
		: Module(Me)
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

	in_addr addy1;
#ifdef IPV6
	in6_addr addy2;
	if ((inet_aton(this->host, &addy1) > 0) || (inet_pton(AF_INET6, this->host, &addy2) > 0))
#else
	if (inet_aton(this->host, &addy1) > 0)
#endif
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
	url.protocol = "http";

	irc::sepstream tokenizer(iurl, '/');
	
	for (int p = 0;; p++)
	{
		std::string part = tokenizer.GetToken();
		if (part.empty() && tokenizer.StreamEnd())
			break;
		
		if ((p == 0) && (part[part.length() - 1] == ':'))
		{
			// Protocol ('http:')
			url.protocol = part.substr(0, part.length() - 1);
		}
		else if ((p == 1) && (part.empty()))
		{
			continue;
		}
		else if (url.domain.empty())
		{
			// Domain part: [user[:pass]@]domain[:port]
			std::string::size_type usrpos = part.find('@');
			if (usrpos != std::string::npos)
			{
				// Have a user (and possibly password) part
				std::string::size_type ppos = part.find(':');
				if ((ppos != std::string::npos) && (ppos < usrpos))
				{
					// Have password too
					url.password = part.substr(ppos + 1, usrpos - ppos - 1);
					url.username = part.substr(0, ppos);
				}
				else
				{
					url.username = part.substr(0, usrpos);
				}
				
				part = part.substr(usrpos + 1);
			}
			
			std::string::size_type popos = part.rfind(':');
			if (popos != std::string::npos)
			{
				url.port = atoi(part.substr(popos + 1).c_str());
				url.domain = part.substr(0, popos);
			}
			else
			{
				url.domain = part;
			}
		}
		else
		{
			// Request (part of it)..
			url.request.append("/");
			url.request.append(part);
		}
	}
	
	if (url.request.empty())
		url.request = "/";

	if ((url.domain.empty()) || (!url.port) || (url.protocol.empty()))
	{
		Instance->Log(DEFAULT, "Invalid URL (%s): Missing required value", iurl.c_str());
		return false;
	}
	
	if (url.protocol != "http")
	{
		Instance->Log(DEFAULT, "Invalid URL (%s): Unsupported protocol '%s'", iurl.c_str(), url.protocol.c_str());
		return false;
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
				this->buffer.clear();
				break;
			}

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
				response->AddHeader(line.substr(0, pos), line.substr(pos + 1));
			}
			else
			{
				continue;
			}
		}
	}
	else
	{
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

MODULE_INIT(ModuleHTTPClient)
