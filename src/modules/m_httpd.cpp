/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *		       E-mail:
 *		<brain@chatspike.net>
 *	   	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include <stdio.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspsocket.h"
#include "helperfuncs.h"
#include "httpd.h"

/* $ModDesc: Provides HTTP serving facilities to modules */

class ModuleHttp;

static Server *Srv;
static ModuleHttp* HttpModule;
extern time_t TIME;
static bool claimed;

enum HttpState
{
	HTTP_LISTEN = 0,
	HTTP_SERVE_WAIT_REQUEST = 1,
	HTTP_SERVE_SEND_DATA = 2
};

class HttpSocket : public InspSocket
{
	FileReader* index;
	HttpState InternalState;
	std::stringstream headers;

 public:

	HttpSocket(std::string host, int port, bool listening, unsigned long maxtime, FileReader* index_page) : InspSocket(host, port, listening, maxtime), index(index_page)
	{
		log(DEBUG,"HttpSocket constructor");
		InternalState = HTTP_LISTEN;
	}

	HttpSocket(int newfd, char* ip, FileReader* ind) : InspSocket(newfd, ip), index(ind)
	{
		InternalState = HTTP_SERVE_WAIT_REQUEST;
	}

	virtual int OnIncomingConnection(int newsock, char* ip)
	{
		if (InternalState == HTTP_LISTEN)
		{
			HttpSocket* s = new HttpSocket(newsock, ip, index);
			Srv->AddSocket(s);
		}
		return true;
	}

	virtual void OnClose()
	{
	}

	std::string Response(int response)
	{
		switch (response)
		{
			case 100:
				return "CONTINUE";
			case 101:
				return "SWITCHING PROTOCOLS";
			case 200:
				return "OK";
			case 201:
				return "CREATED";
			case 202:
				return "ACCEPTED";
			case 203:
				return "NON-AUTHORITATIVE INFORMATION";
			case 204:
				return "NO CONTENT";
			case 205:
				return "RESET CONTENT";
			case 206:
				return "PARTIAL CONTENT";
			case 300:
				return "MULTIPLE CHOICES";
			case 301:
				return "MOVED PERMENANTLY";
			case 302:
				return "FOUND";
			case 303:
				return "SEE OTHER";
			case 304:
				return "NOT MODIFIED";
			case 305:
				return "USE PROXY";
			case 307:
				return "TEMPORARY REDIRECT";
			case 400:
				return "BAD REQUEST";
			case 401:
				return "UNAUTHORIZED";
			case 402:
				return "PAYMENT REQUIRED";
			case 403:
				return "FORBIDDEN";
			case 404:
				return "NOT FOUND";
			case 405:
				return "METHOD NOT ALLOWED";
			case 406:
				return "NOT ACCEPTABLE";
			case 407:
				return "PROXY AUTHENTICATION REQUIRED";
			case 408:
				return "REQUEST TIMEOUT";
			case 409:
				return "CONFLICT";
			case 410:
				return "GONE";
			case 411:
				return "LENGTH REQUIRED";
			case 412:
				return "PRECONDITION FAILED";
			case 413:
				return "REQUEST ENTITY TOO LARGE";
			case 414:
				return "REQUEST-URI TOO LONG";
			case 415:
				return "UNSUPPORTED MEDIA TYPE";
			case 416:
				return "REQUESTED RANGE NOT SATISFIABLE";
			case 417:
				return "EXPECTATION FAILED";
			case 500:
				return "INTERNAL SERVER ERROR";
			case 501:
				return "NOT IMPLEMENTED";
			case 502:
				return "BAD GATEWAY";
			case 503:
				return "SERVICE UNAVAILABLE";
			case 504:
				return "GATEWAY TIMEOUT";
			case 505:
				return "HTTP VERSION NOT SUPPORTED";
			default:
				return "WTF";
			break;
				
		}
	}

	void SendHeaders(unsigned long size, int response, const std::string &extraheaders)
	{
		struct tm *timeinfo = localtime(&TIME);
		this->Write("HTTP/1.1 "+ConvToStr(response)+" "+Response(response)+"\r\nDate: ");
		this->Write(asctime(timeinfo));
		this->Write(extraheaders);
		this->Write("Server: InspIRCd/m_httpd.so/1.1\r\nContent-Length: "+ConvToStr(size)+
				"\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n");
	}

	virtual bool OnDataReady()
	{
		char* data = this->Read();
		std::string request_type;
		std::string uri;
		std::string http_version;

		/* Check that the data read is a valid pointer and it has some content */
		if (data && *data)
		{
			headers << data;

			if (headers.str().find("\r\n\r\n") != std::string::npos)
			{
				/* Headers are complete */
				InternalState = HTTP_SERVE_SEND_DATA;

				headers >> request_type;
				headers >> uri;
				headers >> http_version;

				if ((http_version != "HTTP/1.1") && (http_version != "HTTP/1.0"))
				{
					SendHeaders(0, 505, "");
				}
				else
				{
					if ((request_type == "GET") && (uri == "/"))
					{
						SendHeaders(index->ContentSize(), 200, "");
						this->Write(index->Contents());
					}
					else
					{
						claimed = false;
						HTTPRequest httpr(request_type,uri,&headers,this,this->GetIP());
						Event e((char*)&httpr, (Module*)HttpModule, "httpd_url");
						e.Send();

						if (!claimed)
						{
							SendHeaders(0, 404, "");
							log(DEBUG,"Page not claimed, 404");
						}
					}
				}

				return false;
			}
			return true;
		}
		else
		{
			/* Bastard client closed the socket on us!
			 * Oh wait, theyre SUPPOSED to do that!
			 */
			return false;
		}
	}

	void Page(std::stringstream* n, int response, std::string& extraheaders)
	{
		log(DEBUG,"Sending page");
		SendHeaders(n->str().length(), response, extraheaders);
		this->Write(n->str());
	}
};

class ModuleHttp : public Module
{
	int port;
	std::string host;
	std::string bindip;
	std::string indexfile;

	FileReader index;

	HttpSocket* http;

 public:

	void ReadConfig()
	{
		ConfigReader c;
		this->host = c.ReadValue("http", "host", 0);
		this->bindip = c.ReadValue("http", "ip", 0);
		this->port = c.ReadInteger("http", "port", 0, true);
		this->indexfile = c.ReadValue("http", "index", 0);

		index.LoadFile(this->indexfile);
	}

	void CreateListener()
	{
		http = new HttpSocket(this->bindip, this->port, true, 0, &index);
		if ((http) && (http->GetState() == I_LISTENING))
		{
			Srv->AddSocket(http);
		}
	}

	ModuleHttp(Server* Me) : Module::Module(Me)
	{
		Srv = Me;
		ReadConfig();
		CreateListener();
	}

	void OnEvent(Event* event)
	{
	}

	char* OnRequest(Request* request)
	{
		log(DEBUG,"Got HTTPDocument object");
		claimed = true;
		HTTPDocument* doc = (HTTPDocument*)request->GetData();
		HttpSocket* sock = (HttpSocket*)doc->sock;
		sock->Page(doc->GetDocument(), doc->GetResponseCode(), doc->GetExtraHeaders());
		return NULL;
	}

	void Implements(char* List)
	{
		List[I_OnEvent] = List[I_OnRequest] = 1;
	}

	virtual ~ModuleHttp()
	{
		Srv->DelSocket(http);
	}

	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
	}
};


class ModuleHttpFactory : public ModuleFactory
{
 public:
	ModuleHttpFactory()
	{
	}
	
	~ModuleHttpFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		HttpModule = new ModuleHttp(Me);
		return HttpModule;
	}
};


extern "C" void * init_module( void )
{
	return new ModuleHttpFactory;
}
