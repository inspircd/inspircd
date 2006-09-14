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
#include "inspircd.h"
#include "httpd.h"

/* $ModDesc: Provides HTTP serving facilities to modules */

class ModuleHttp;

static ModuleHttp* HttpModule;
static bool claimed;

enum HttpState
{
	HTTP_LISTEN = 0,
	HTTP_SERVE_WAIT_REQUEST = 1,
	HTTP_SERVE_RECV_POSTDATA = 2,
	HTTP_SERVE_SEND_DATA = 3
};

class HttpSocket : public InspSocket
{
	FileReader* index;
	HttpState InternalState;
	std::stringstream headers;
	std::string postdata;
	std::string request_type;
	std::string uri;
	std::string http_version;
	int postsize;
	int amount;

 public:

	HttpSocket(InspIRCd* SI, std::string host, int port, bool listening, unsigned long maxtime, FileReader* index_page) : InspSocket(SI, host, port, listening, maxtime), index(index_page), postsize(0)
	{
		SI->Log(DEBUG,"HttpSocket constructor");
		InternalState = HTTP_LISTEN;
	}

	HttpSocket(InspIRCd* SI, int newfd, char* ip, FileReader* ind) : InspSocket(SI, newfd, ip), index(ind), postsize(0)
	{
		InternalState = HTTP_SERVE_WAIT_REQUEST;
	}

	virtual int OnIncomingConnection(int newsock, char* ip)
	{
		if (InternalState == HTTP_LISTEN)
		{
			HttpSocket* s = new HttpSocket(this->Instance, newsock, ip, index);
			s = s; /* Stop GCC whining */
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
		time_t local = this->Instance->Time();
		struct tm *timeinfo = localtime(&local);
		this->Write("HTTP/1.1 "+ConvToStr(response)+" "+Response(response)+"\r\nDate: ");
		this->Write(asctime(timeinfo));
		if (extraheaders.empty())
		{
			this->Write("Content-Type: text/html\r\n");
		}
		else
		{
			this->Write(extraheaders);
		}
		this->Write("Server: InspIRCd/m_httpd.so/1.1\r\nContent-Length: "+ConvToStr(size)+
				"\r\nConnection: close\r\n\r\n");
	}

	virtual bool OnDataReady()
	{
		char* data = this->Read();

		/* Check that the data read is a valid pointer and it has some content */
		if (data && *data)
		{
			headers << data;

			if (headers.str().find("\r\n\r\n") != std::string::npos)
			{
				headers >> request_type;
				headers >> uri;
				headers >> http_version;

				if ((InternalState == HTTP_SERVE_WAIT_REQUEST) && (request_type == "POST"))
				{
					/* Do we need to fetch postdata? */
					postdata = "";
					amount = 0;
					InternalState = HTTP_SERVE_RECV_POSTDATA;
					std::string header_item;
					while (headers >> header_item)
					{
						if (header_item == "Content-Length:")
						{
							headers >> header_item;
							postsize = atoi(header_item.c_str());
						}
					}
					Instance->Log(DEBUG,"%d bytes to read for POST",postsize);
					/* Get content length and store */
				}
				else if (InternalState == HTTP_SERVE_RECV_POSTDATA)
				{
					/* Add postdata, once we have it all, send the event */
					amount += strlen(data);
					postdata.append(data);
					if (amount >= postsize)
					{
						InternalState = HTTP_SERVE_SEND_DATA;

						if ((http_version != "HTTP/1.1") && (http_version != "HTTP/1.0"))
						{
							SendHeaders(0, 505, "");
						}
						else
						{
							claimed = false;
							HTTPRequest httpr(request_type,uri,&headers,this,this->GetIP(),postdata);
							Event e((char*)&httpr, (Module*)HttpModule, "httpd_url");
							e.Send(this->Instance);

							if (!claimed)
							{
								SendHeaders(0, 404, "");
								Instance->Log(DEBUG,"Page not claimed, 404");
							}
							
						}
					}
				}
				else
				{
					/* Headers are complete */
					InternalState = HTTP_SERVE_SEND_DATA;
	
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
							HTTPRequest httpr(request_type,uri,&headers,this,this->GetIP(),postdata);
							Event e((char*)&httpr, (Module*)HttpModule, "httpd_url");
							e.Send(this->Instance);
	
							if (!claimed)
							{
								SendHeaders(0, 404, "");
								Instance->Log(DEBUG,"Page not claimed, 404");
							}
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
		Instance->Log(DEBUG,"Sending page");
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

	FileReader* index;

	HttpSocket* http;

 public:

	void ReadConfig()
	{
		ConfigReader c(ServerInstance);
		this->host = c.ReadValue("http", "host", 0);
		this->bindip = c.ReadValue("http", "ip", 0);
		this->port = c.ReadInteger("http", "port", 0, true);
		this->indexfile = c.ReadValue("http", "index", 0);

		if (index)
		{
			delete index;
			index = NULL;
		}
		index = new FileReader(ServerInstance, this->indexfile);
	}

	void CreateListener()
	{
		http = new HttpSocket(ServerInstance, this->bindip, this->port, true, 0, index);
	}

	ModuleHttp(InspIRCd* Me) : Module::Module(Me)
	{
		index = NULL;
		ReadConfig();
		CreateListener();
	}

	void OnEvent(Event* event)
	{
	}

	char* OnRequest(Request* request)
	{
		ServerInstance->Log(DEBUG,"Got HTTPDocument object");
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
		ServerInstance->SE->DelFd(http);
	}

	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR|VF_SERVICEPROVIDER);
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		HttpModule = new ModuleHttp(Me);
		return HttpModule;
	}
};


extern "C" void * init_module( void )
{
	return new ModuleHttpFactory;
}
