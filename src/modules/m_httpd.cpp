/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Peter Powell <petpow@saberuk.com>
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
static bool claimed;
static std::set<HttpServerSocket*> sockets;

#define MAX_REQUEST_SIZE 8192

/** HTTP socket states
 */
enum HttpState
{
	// Waiting for the GET/POST/etc request.
	HTTP_REQUEST_WAITING_METHOD,

	// Waiting for the end of the C2S headers.
	HTTP_REQUEST_WAITING_HEADERS,

	// Waiting for the end of the C2S request data.
	HTTP_REQUEST_WAITING_FORMDATA,

	// Waiting for the response to send.
	HTTP_RESPONSE_SENDING
};

/** A socket used for HTTP transport
 */
class HttpServerSocket : public BufferedSocket
{
	HttpState InternalState;
	HTTPHeaders headers;
	long form_data_remaining;
	std::string form_data;
	std::string request_type;
	std::string uri;
	std::string http_version;
	irc::sockets::sockaddrs* sockaddr;

 public:
	const time_t createtime;

	HttpServerSocket(int newfd, ListenSocket* via, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server)
		: BufferedSocket(newfd)
		, sockaddr(client)
		, createtime(ServerInstance->Time())
	{
		InternalState = HTTP_REQUEST_WAITING_METHOD;

		if (via->iohookprov)
			via->iohookprov->OnAccept(this, client, server);
	}

	~HttpServerSocket()
	{
		sockets.erase(this);
	}

	void OnError(BufferedSocketError) CXX11_OVERRIDE
	{
		ServerInstance->GlobalCulls.AddItem(this);
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
		Close();
	}

	void SendHeaders(unsigned long size, int response, HTTPHeaders &rheaders)
	{
		// Default to a safe version if stuff is broken.
		if (http_version.empty() || response == 505)
			http_version = "HTTP/1.0";

		WriteData(http_version + " " + ConvToStr(response) + " " + Response(response) + "\r\n");

		rheaders.CreateHeader("Date", InspIRCd::TimeString(ServerInstance->Time()));
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
		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "OnDataReady: bytes %lu, state %d", recvq.size(), InternalState);
		switch (InternalState)
		{
			case HTTP_REQUEST_WAITING_METHOD:
				TryParseMethod();
				break;
			case HTTP_REQUEST_WAITING_HEADERS:
				TryParseHeaders();
				break;
			case HTTP_REQUEST_WAITING_FORMDATA:
				TryParseFormData();
				break;
			default:
				ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "BUG: OnDataReady was called with an unknown state: %d", InternalState);
				SendHTTPError(500);
				break;
		}
	}

	void TryParseMethod()
	{
		std::string::size_type eol = recvq.find("\r\n");
		if (eol == std::string::npos)
			return;

		// Attempt to parse the tokens from the receive queue.
		irc::spacesepstream stream(recvq.substr(0, eol));
		if (!stream.GetToken(request_type) || !stream.GetToken(uri) || !stream.GetToken(http_version) || stream.GetRemaining() != "")
		{
			// The client has sent something which doesn't look like a HTTP request. ABANDON SHIP!
			SendHTTPError(400);
			return;
		}
		else if (http_version != "HTTP/1.0" && http_version != "HTTP/1.1")
		{
			// The client has requested a HTTP version we don't support. ABANDON SHIP!
			SendHTTPError(505);
			return;
		}

		// Upper case the method and HTTP version to make matching easier.
		std::transform(request_type.begin(), request_type.end(), request_type.begin(), ::toupper);
		std::transform(http_version.begin(), http_version.end(), http_version.begin(), ::toupper);

		// Strip trailing path separators.
		if (uri.size() > 1)
			uri = uri.substr(0, uri.find_last_not_of('/') + 1);

		// Remove the data we have parsed from the receive queue and advance to the next stage.
		recvq.erase(0, eol + 2);
		InternalState = HTTP_REQUEST_WAITING_HEADERS;
		TryParseHeaders();
	}

	void TryParseHeaders()
	{
		// Attempt to parse the headers from the receive queue.
		std::string::size_type eol;
		while ((eol = recvq.find("\r\n")) != std::string::npos)
		{
			std::string header = recvq.substr(0, eol);
			recvq.erase(0, eol + 2);
			if (header.size())
			{
				// Extract the header name and value.
				std::string::size_type separator = header.find(": ");
				if (separator == std::string::npos || separator == 0 || separator == header.length() - 1)
				{
					// The client has sent something which doesn't look like a HTTP header. ABANDON SHIP!
					SendHTTPError(400);
					return;
				}
				headers.SetHeader(header.substr(0, separator), header.substr(separator + 2));
			}
			else
			{
				// We have reached the end of the headers.
				if (headers.IsSet("Content-Length"))
				{
					form_data_remaining = ConvToInt(headers.GetHeader("Content-Length"));
					if (form_data_remaining <= 0 || form_data_remaining >= MAX_REQUEST_SIZE)
					{
						// The client is trying to upload something massive. ABANDON SHIP!
						SendHTTPError(413);
						return;
					}

					InternalState = HTTP_REQUEST_WAITING_FORMDATA;
					TryParseFormData();
				}
				else
				{
					// Nothing left to do other than to wait for the response to send.
					InternalState = HTTP_RESPONSE_SENDING;
					this->HandleRequest();
				}
				break;
			}
		}
	}

	void TryParseFormData()
	{
		if (form_data_remaining && recvq.length())
		{
			std::string data = recvq.substr(0, form_data_remaining);
			form_data.append(data);
			form_data_remaining -= data.size();
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "TryParseFormData: %li bytes stored, %li remaining.",
				data.size(), form_data_remaining);
		}
		if (!form_data_remaining)
		{
			// Nothing left to do other than to wait for the response to send.
			InternalState = HTTP_RESPONSE_SENDING;
			this->HandleRequest();
		}
	}

	void HandleRequest()
	{
		claimed = false;
		HTTPRequest acl((Module*)HttpModule, "httpd_acl", request_type, uri, &headers, this, sockaddr->addr(), form_data);
		acl.Send();

		if (!claimed)
		{
			HTTPRequest url((Module*)HttpModule, "httpd_url", request_type, uri, &headers, this, sockaddr->addr(), form_data);
			url.Send();
	
			if (!claimed)
				SendHTTPError(404);
		}

		// Request finished. Prepare for the next request.
		InternalState = HTTP_REQUEST_WAITING_METHOD;
		TryParseMethod();
	}

	void Page(std::stringstream* n, int response, HTTPHeaders *hheaders)
	{
		SendHeaders(n->str().length(), response, *hheaders);
		WriteData(n->str());
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
		claimed = true;
		resp.src.sock->Page(resp.document, resp.responsecode, &resp.headers);
	}
};

class ModuleHttpServer : public Module
{
	std::vector<HttpServerSocket *> httpsocks;
	HTTPdAPIImpl APIImpl;
	unsigned int timeoutsec;

 public:
	ModuleHttpServer()
		: APIImpl(this)
	{
	}

	void init() CXX11_OVERRIDE
	{
		HttpModule = this;
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("httpd");
		timeoutsec = tag->getInt("timeout");
	}

	ModResult OnAcceptConnection(int nfd, ListenSocket* from, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server) CXX11_OVERRIDE
	{
		if (from->bind_tag->getString("type") != "httpd")
			return MOD_RES_PASSTHRU;
		sockets.insert(new HttpServerSocket(nfd, from, client, server));
		return MOD_RES_ALLOW;
	}

	void OnBackgroundTimer(time_t curtime) CXX11_OVERRIDE
	{
		if (!timeoutsec)
			return;

		time_t oldest_allowed = curtime - timeoutsec;
		for (std::set<HttpServerSocket*>::const_iterator i = sockets.begin(); i != sockets.end(); )
		{
			HttpServerSocket* sock = *i;
			++i;
			if (sock->createtime < oldest_allowed)
			{
				sock->cull();
				delete sock;
			}
		}
	}

	CullResult cull() CXX11_OVERRIDE
	{
		std::set<HttpServerSocket*> local;
		local.swap(sockets);
		for (std::set<HttpServerSocket*>::const_iterator i = local.begin(); i != local.end(); ++i)
		{
			HttpServerSocket* sock = *i;
			sock->cull();
			delete sock;
		}
		return Module::cull();
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides HTTP serving facilities to modules", VF_VENDOR);
	}
};

MODULE_INIT(ModuleHttpServer)
