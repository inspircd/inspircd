/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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
#include "iohook.h"
#include "modules/httpd.h"

class ModuleHttpServer;

static ModuleHttpServer* HttpModule;
static insp::intrusive_list<HttpServerSocket> sockets;
static Events::ModuleEventProvider* aclevprov;
static Events::ModuleEventProvider* reqevprov;

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
class HttpServerSocket : public BufferedSocket, public Timer, public insp::intrusive_list_node<HttpServerSocket>
{
	HttpState InternalState;
	std::string ip;

	HTTPHeaders headers;
	std::string reqbuffer;
	std::string postdata;
	unsigned int postsize;
	std::string request_type;
	std::string uri;
	std::string http_version;

	/** True if this object is in the cull list
	 */
	bool waitingcull;

	bool Tick(time_t currtime) CXX11_OVERRIDE
	{
		AddToCull();
		return false;
	}

 public:
	HttpServerSocket(int newfd, const std::string& IP, ListenSocket* via, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server, unsigned int timeoutsec)
		: BufferedSocket(newfd)
		, Timer(timeoutsec)
		, InternalState(HTTP_SERVE_WAIT_REQUEST)
		, ip(IP)
		, postsize(0)
		, waitingcull(false)
	{
		if ((!via->iohookprovs.empty()) && (via->iohookprovs.back()))
		{
			via->iohookprovs.back()->OnAccept(this, client, server);
			// IOHook may have errored
			if (!getError().empty())
			{
				AddToCull();
				return;
			}
		}

		ServerInstance->Timers.AddTimer(this);
	}

	~HttpServerSocket()
	{
		sockets.erase(this);
	}

	void OnError(BufferedSocketError) CXX11_OVERRIDE
	{
		AddToCull();
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
				return "MOVED PERMANENTLY";
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
		WriteData(data);
	}

	void SendHeaders(unsigned long size, int response, HTTPHeaders &rheaders)
	{

		WriteData(http_version + " "+ConvToStr(response)+" "+Response(response)+"\r\n");

		rheaders.CreateHeader("Date", InspIRCd::TimeString(ServerInstance->Time(), "%a, %d %b %Y %H:%M:%S GMT", true));
		rheaders.CreateHeader("Server", INSPIRCD_BRANCH);
		rheaders.SetHeader("Content-Length", ConvToStr(size));

		if (size)
			rheaders.CreateHeader("Content-Type", "text/html");
		else
			rheaders.RemoveHeader("Content-Type");

		/* Supporting Connection: keep-alive causes a whole world of hurt syncronizing timeouts,
		 * so remove it, its not essential for what we need.
		 */
		rheaders.SetHeader("Connection", "Close");

		WriteData(rheaders.GetFormattedHeaders());
		WriteData("\r\n");
	}

	void OnDataReady()
	{
		if (InternalState == HTTP_SERVE_RECV_POSTDATA)
		{
			postdata.append(recvq);
			if (postdata.length() >= postsize)
				ServeData();
		}
		else
		{
			reqbuffer.append(recvq);

			if (reqbuffer.length() >= 8192)
			{
				ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "m_httpd dropped connection due to an oversized request buffer");
				reqbuffer.clear();
				SetError("Buffer");
			}

			if (InternalState == HTTP_SERVE_WAIT_REQUEST)
				CheckRequestBuffer();
		}
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

			std::string cheader(reqbuffer, hbegin, hend - hbegin);

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

		if (headers.IsSet("Content-Length") && (postsize = ConvToInt(headers.GetHeader("Content-Length"))) > 0)
		{
			InternalState = HTTP_SERVE_RECV_POSTDATA;

			if (reqbuffer.length() >= postsize)
			{
				postdata.assign(reqbuffer, 0, postsize);
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

		ModResult MOD_RESULT;
		HTTPRequest acl(request_type, uri, &headers, this, ip, postdata);
		FIRST_MOD_RESULT_CUSTOM(*aclevprov, HTTPACLEventListener, OnHTTPACLCheck, MOD_RESULT, (acl));
		if (MOD_RESULT != MOD_RES_DENY)
		{
			HTTPRequest url(request_type, uri, &headers, this, ip, postdata);
			FIRST_MOD_RESULT_CUSTOM(*reqevprov, HTTPRequestEventListener, OnHTTPRequest, MOD_RESULT, (url));
			if (MOD_RESULT == MOD_RES_PASSTHRU)
			{
				SendHTTPError(404);
			}
		}
	}

	void Page(std::stringstream* n, int response, HTTPHeaders *hheaders)
	{
		SendHeaders(n->str().length(), response, *hheaders);
		WriteData(n->str());
	}

	void AddToCull()
	{
		if (waitingcull)
			return;

		waitingcull = true;
		Close();
		ServerInstance->GlobalCulls.AddItem(this);
	}
};

class HTTPdAPIImpl : public HTTPdAPIBase
{
 public:
	HTTPdAPIImpl(Module* parent)
		: HTTPdAPIBase(parent)
	{
	}

	void SendResponse(HTTPDocumentResponse& resp) CXX11_OVERRIDE
	{
		resp.src.sock->Page(resp.document, resp.responsecode, &resp.headers);
	}
};

class ModuleHttpServer : public Module
{
	HTTPdAPIImpl APIImpl;
	unsigned int timeoutsec;
	Events::ModuleEventProvider acleventprov;
	Events::ModuleEventProvider reqeventprov;

 public:
	ModuleHttpServer()
		: APIImpl(this)
		, acleventprov(this, "event/http-acl")
		, reqeventprov(this, "event/http-request")
	{
		aclevprov = &acleventprov;
		reqevprov = &reqeventprov;
	}

	void init() CXX11_OVERRIDE
	{
		HttpModule = this;
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("httpd");
		timeoutsec = tag->getInt("timeout", 10, 1);
	}

	ModResult OnAcceptConnection(int nfd, ListenSocket* from, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server) CXX11_OVERRIDE
	{
		if (from->bind_tag->getString("type") != "httpd")
			return MOD_RES_PASSTHRU;
		int port;
		std::string incomingip;
		irc::sockets::satoap(*client, incomingip, port);
		sockets.push_front(new HttpServerSocket(nfd, incomingip, from, client, server, timeoutsec));
		return MOD_RES_ALLOW;
	}

	void OnUnloadModule(Module* mod)
	{
		for (insp::intrusive_list<HttpServerSocket>::const_iterator i = sockets.begin(); i != sockets.end(); )
		{
			HttpServerSocket* sock = *i;
			++i;
			if (sock->GetModHook(mod))
			{
				sock->cull();
				delete sock;
			}
		}
	}

	CullResult cull() CXX11_OVERRIDE
	{
		for (insp::intrusive_list<HttpServerSocket>::const_iterator i = sockets.begin(); i != sockets.end(); ++i)
		{
			HttpServerSocket* sock = *i;
			sock->AddToCull();
		}
		return Module::cull();
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides HTTP serving facilities to modules", VF_VENDOR);
	}
};

MODULE_INIT(ModuleHttpServer)
