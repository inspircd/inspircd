/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2006-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 John Brooks <john.brooks@dereferenced.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "httpd.h"

/* $ModDesc: Provides HTTP serving facilities to modules */
/* $ModDep: httpd.h */

class ModuleHttpServer;

static ModuleHttpServer* HttpModule;
static bool claimed;

/** HTTP socket states
 */
enum HttpState
{
	HTTP_SERVE_WAIT_REQUEST = 0, /* Waiting for a full request */
	HTTP_SERVE_RECV_POSTDATA = 1, /* Waiting to finish recieving POST data */
	HTTP_SERVE_SEND_DATA = 2 /* Sending response */
};

/** A socket used for HTTP transport
 */
class HttpServerSocket : public BufferedSocket
{
	FileReader* index;
	HttpState InternalState;

	HTTPHeaders headers;
	std::string reqbuffer;
	std::string postdata;
	unsigned int postsize;
	std::string request_type;
	std::string uri;
	std::string http_version;

 public:

	HttpServerSocket(InspIRCd* SI, int newfd, char* ip, FileReader* ind) : BufferedSocket(SI, newfd, ip), index(ind), postsize(0)
	{
		InternalState = HTTP_SERVE_WAIT_REQUEST;
	}

	FileReader* GetIndex()
	{
		return index;
	}

	~HttpServerSocket()
	{
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

	void SendHTTPError(int response)
	{
		HTTPHeaders empty;
		std::string data = "<html><head></head><body>Server error "+ConvToStr(response)+": "+Response(response)+"<br>"+
		                   "<small>Powered by <a href='http://www.inspircd.org'>InspIRCd</a></small></body></html>";

		SendHeaders(data.length(), response, empty);
		this->Write(data);
	}

	void SendHeaders(unsigned long size, int response, HTTPHeaders &rheaders)
	{

		this->Write(http_version + " "+ConvToStr(response)+" "+Response(response)+"\r\n");

		time_t local = this->ServerInstance->Time();
		struct tm *timeinfo = gmtime(&local);
		char *date = asctime(timeinfo);
		date[strlen(date) - 1] = '\0';
		rheaders.CreateHeader("Date", date);

		rheaders.CreateHeader("Server", "InspIRCd/m_httpd.so/1.2");
		rheaders.SetHeader("Content-Length", ConvToStr(size));

		if (size)
			rheaders.CreateHeader("Content-Type", "text/html");
		else
			rheaders.RemoveHeader("Content-Type");

		/* Supporting Connection: keep-alive causes a whole world of hurt syncronizing timeouts,
		 * so remove it, its not essential for what we need.
		 */
		rheaders.SetHeader("Connection", "Close");

		this->Write(rheaders.GetFormattedHeaders());
		this->Write("\r\n");
	}

	virtual bool OnDataReady()
	{
		const char* data = this->Read();

		/* Check that the data read is a valid pointer and it has some content */
		if (!data || !*data)
			return false;

		if (InternalState == HTTP_SERVE_RECV_POSTDATA)
		{
			postdata.append(data);
			if (postdata.length() >= postsize)
				ServeData();
		}
		else
		{
			reqbuffer.append(data);

			if (reqbuffer.length() >= 8192)
			{
				ServerInstance->Logs->Log("m_httpd",DEBUG, "m_httpd dropped connection due to an oversized request buffer");
				reqbuffer.clear();
				return false;
			}

			if (InternalState == HTTP_SERVE_WAIT_REQUEST)
				CheckRequestBuffer();
		}

		return true;
	}

	void CheckRequestBuffer()
	{
		std::string::size_type reqend = reqbuffer.find("\r\n\r\n");
		if (reqend == std::string::npos)
			return;

		// We have the headers; parse them all
		std::string::size_type hbegin = 0, hend;
		while ((hend = reqbuffer.find("\r\n", hbegin)) != std::string::npos)
		{
			if (hbegin == hend)
				break;

			if (request_type.empty())
			{
				std::istringstream cheader(std::string(reqbuffer, hbegin, hend - hbegin));
				cheader >> request_type;
				cheader >> uri;
				cheader >> http_version;

				if (request_type.empty() || uri.empty() || http_version.empty())
				{
					SendHTTPError(400);
					return;
				}

				hbegin = hend + 2;
				continue;
			}

			std::string cheader = reqbuffer.substr(hbegin, hend - hbegin);

			std::string::size_type fieldsep = cheader.find(':');
			if ((fieldsep == std::string::npos) || (fieldsep == 0) || (fieldsep == cheader.length() - 1))
			{
				SendHTTPError(400);
				return;
			}

			headers.SetHeader(cheader.substr(0, fieldsep), cheader.substr(fieldsep + 2));

			hbegin = hend + 2;
		}

		reqbuffer.erase(0, reqend + 4);

		std::transform(request_type.begin(), request_type.end(), request_type.begin(), ::toupper);
		std::transform(http_version.begin(), http_version.end(), http_version.begin(), ::toupper);

		if ((http_version != "HTTP/1.1") && (http_version != "HTTP/1.0"))
		{
			SendHTTPError(505);
			return;
		}

		if (headers.IsSet("Content-Length") && (postsize = atoi(headers.GetHeader("Content-Length").c_str())) != 0)
		{
			InternalState = HTTP_SERVE_RECV_POSTDATA;

			if (reqbuffer.length() >= postsize)
			{
				postdata = reqbuffer.substr(0, postsize);
				reqbuffer.erase(0, postsize);
			}
			else if (!reqbuffer.empty())
			{
				postdata = reqbuffer;
				reqbuffer.clear();
			}

			if (postdata.length() >= postsize)
				ServeData();

			return;
		}

		ServeData();
	}

	void ServeData()
	{
		InternalState = HTTP_SERVE_SEND_DATA;

		if ((request_type == "GET") && (uri == "/"))
		{
			HTTPHeaders empty;
			SendHeaders(index->ContentSize(), 200, empty);
			this->Write(index->Contents());
		}
		else
		{
			claimed = false;
			HTTPRequest httpr(request_type,uri,&headers,this,this->GetIP(),postdata);
			Event acl((char*)&httpr, (Module*)HttpModule, "httpd_acl");
			acl.Send(this->ServerInstance);
			if (!claimed)
			{
				Event e((char*)&httpr, (Module*)HttpModule, "httpd_url");
				e.Send(this->ServerInstance);
				if (!claimed)
				{
					SendHTTPError(404);
				}
			}
		}
	}

	void Page(std::stringstream* n, int response, HTTPHeaders *hheaders)
	{
		SendHeaders(n->str().length(), response, *hheaders);
		this->Write(n->str());
	}
};

/** Spawn HTTP sockets from a listener
 */
class HttpListener : public ListenSocketBase
{
	FileReader* index;

 public:
	HttpListener(InspIRCd* Instance, FileReader *idx, int port, const std::string &addr) : ListenSocketBase(Instance, port, addr)
	{
		this->index = idx;
	}

	virtual void OnAcceptReady(const std::string &ipconnectedto, int nfd, const std::string &incomingip)
	{
		new HttpServerSocket(ServerInstance, nfd, (char *)incomingip.c_str(), index); // ugly cast courtesy of bufferedsocket
	}
};

class ModuleHttpServer : public Module
{
	std::vector<HttpServerSocket *> httpsocks;
	std::vector<HttpListener *> httplisteners;
 public:

	void ReadConfig()
	{
		int port;
		std::string host;
		std::string bindip;
		std::string indexfile;
		FileReader* index;
		HttpListener *http;
		ConfigReader c(ServerInstance);

		httpsocks.clear(); // XXX this will BREAK if this module is made rehashable
		httplisteners.clear();

		for (int i = 0; i < c.Enumerate("http"); i++)
		{
			host = c.ReadValue("http", "host", i);
			bindip = c.ReadValue("http", "ip", i);
			port = c.ReadInteger("http", "port", i, true);
			indexfile = c.ReadValue("http", "index", i);
			index = new FileReader(ServerInstance, indexfile);
			if (!index->Exists())
				throw ModuleException("Can't read index file: "+indexfile);
			http = new HttpListener(ServerInstance, index, port, (char *)bindip.c_str()); // XXX this cast SUCKS.
			httplisteners.push_back(http);
		}
	}

	ModuleHttpServer(InspIRCd* Me) : Module(Me)
	{
		ReadConfig();
		HttpModule = this;
		Implementation eventlist[] = { I_OnRequest };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}

	virtual const char* OnRequest(Request* request)
	{
		claimed = true;
		HTTPDocument* doc = (HTTPDocument*)request->GetData();
		HttpServerSocket* sock = (HttpServerSocket*)doc->sock;
		sock->Page(doc->GetDocument(), doc->GetResponseCode(), &doc->headers);
		return NULL;
	}


	virtual ~ModuleHttpServer()
	{
		for (size_t i = 0; i < httplisteners.size(); i++)
		{
			delete httplisteners[i];
		}

		for (size_t i = 0; i < httpsocks.size(); i++)
		{
			ServerInstance->SE->DelFd(httpsocks[i]);
			httpsocks[i]->Close();
			delete httpsocks[i]->GetIndex();
		}
		ServerInstance->BufferedSocketCull();
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR | VF_SERVICEPROVIDER, API_VERSION);
	}
};

MODULE_INIT(ModuleHttpServer)
