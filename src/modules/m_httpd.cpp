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

/// $CompilerFlags: -Ivendor_directory("http_parser")


#include "inspircd.h"
#include "iohook.h"
#include "modules/httpd.h"

#ifdef __GNUC__
# pragma GCC diagnostic push
#endif

// Fix warnings about the use of commas at end of enumerator lists and long long
// on C++03.
#if defined __clang__
# pragma clang diagnostic ignored "-Wc++11-extensions"
# pragma clang diagnostic ignored "-Wc++11-long-long"
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wlong-long"
# if (__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 8))
#  pragma GCC diagnostic ignored "-Wpedantic"
# else
#  pragma GCC diagnostic ignored "-pedantic"
# endif
#endif

// Fix warnings about shadowing in http_parser.
#ifdef __GNUC__
# pragma GCC diagnostic ignored "-Wshadow"
#endif

#include <http_parser.c>

#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif

class ModuleHttpServer;

static ModuleHttpServer* HttpModule;
static insp::intrusive_list<HttpServerSocket> sockets;
static Events::ModuleEventProvider* aclevprov;
static Events::ModuleEventProvider* reqevprov;
static http_parser_settings parser_settings;

/** A socket used for HTTP transport
 */
class HttpServerSocket : public BufferedSocket, public Timer, public insp::intrusive_list_node<HttpServerSocket>
{
 private:
	friend class ModuleHttpServer;

	http_parser parser;
	http_parser_url url;
	std::string ip;
	std::string uri;
	HTTPHeaders headers;
	std::string body;
	size_t total_buffers;
	int status_code;

	/** True if this object is in the cull list
	 */
	bool waitingcull;
	bool messagecomplete;

	bool Tick(time_t currtime) CXX11_OVERRIDE
	{
		if (!messagecomplete)
		{
			AddToCull();
			return false;
		}

		return true;
	}

	template<int (HttpServerSocket::*f)()>
	static int Callback(http_parser* p)
	{
		HttpServerSocket* sock = static_cast<HttpServerSocket*>(p->data);
		return (sock->*f)();
	}

	template<int (HttpServerSocket::*f)(const char*, size_t)>
	static int DataCallback(http_parser* p, const char* buf, size_t len)
	{
		HttpServerSocket* sock = static_cast<HttpServerSocket*>(p->data);
		return (sock->*f)(buf, len);
	}

	static void ConfigureParser()
	{
		http_parser_settings_init(&parser_settings);
		parser_settings.on_message_begin = Callback<&HttpServerSocket::OnMessageBegin>;
		parser_settings.on_url = DataCallback<&HttpServerSocket::OnUrl>;
		parser_settings.on_header_field = DataCallback<&HttpServerSocket::OnHeaderField>;
		parser_settings.on_body = DataCallback<&HttpServerSocket::OnBody>;
		parser_settings.on_message_complete = Callback<&HttpServerSocket::OnMessageComplete>;
	}

	int OnMessageBegin()
	{
		uri.clear();
		header_state = HEADER_NONE;
		body.clear();
		total_buffers = 0;
		return 0;
	}

	bool AcceptData(size_t len)
	{
		total_buffers += len;
		return total_buffers < 8192;
	}

	int OnUrl(const char* buf, size_t len)
	{
		if (!AcceptData(len))
		{
			status_code = HTTP_STATUS_URI_TOO_LONG;
			return -1;
		}
		uri.append(buf, len);
		return 0;
	}

	enum { HEADER_NONE, HEADER_FIELD, HEADER_VALUE } header_state;
	std::string header_field;
	std::string header_value;

	void OnHeaderComplete()
	{
		headers.SetHeader(header_field, header_value);
		header_field.clear();
		header_value.clear();
	}

	int OnHeaderField(const char* buf, size_t len)
	{
		if (header_state == HEADER_VALUE)
			OnHeaderComplete();
		header_state = HEADER_FIELD;
		if (!AcceptData(len))
		{
			status_code = HTTP_STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE;
			return -1;
		}
		header_field.append(buf, len);
		return 0;
	}

	int OnHeaderValue(const char* buf, size_t len)
	{
		header_state = HEADER_VALUE;
		if (!AcceptData(len))
		{
			status_code = HTTP_STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE;
			return -1;
		}
		header_value.append(buf, len);
		return 0;
	}

	int OnHeadersComplete()
	{
		if (header_state != HEADER_NONE)
			OnHeaderComplete();
		return 0;
	}

	int OnBody(const char* buf, size_t len)
	{
		if (!AcceptData(len))
		{
			status_code = HTTP_STATUS_PAYLOAD_TOO_LARGE;
			return -1;
		}
		body.append(buf, len);
		return 0;
	}

	int OnMessageComplete()
	{
		messagecomplete = true;
		ServeData();
		return 0;
	}

 public:
	HttpServerSocket(int newfd, const std::string& IP, ListenSocket* via, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server, unsigned int timeoutsec)
		: BufferedSocket(newfd)
		, Timer(timeoutsec)
		, ip(IP)
		, status_code(0)
		, waitingcull(false)
		, messagecomplete(false)
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

		parser.data = this;
		http_parser_init(&parser, HTTP_REQUEST);
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

	void SendHTTPError(unsigned int response)
	{
		HTTPHeaders empty;
		std::string data = InspIRCd::Format(
			"<html><head></head><body>Server error %u: %s<br>"
			"<small>Powered by <a href='https://www.inspircd.org'>InspIRCd</a></small></body></html>", response, http_status_str((http_status)response));

		Page(data, response, &empty);
	}

	void SendHeaders(unsigned long size, unsigned int response, HTTPHeaders &rheaders)
	{
		WriteData(InspIRCd::Format("HTTP/%u.%u %u %s\r\n", parser.http_major ? parser.http_major : 1, parser.http_major ? parser.http_minor : 1, response, http_status_str((http_status)response)));

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

	void OnDataReady() CXX11_OVERRIDE
	{
		if (parser.upgrade || HTTP_PARSER_ERRNO(&parser))
			return;
		http_parser_execute(&parser, &parser_settings, recvq.data(), recvq.size());
		if (parser.upgrade || HTTP_PARSER_ERRNO(&parser))
			SendHTTPError(status_code ? status_code : 400);
	}

	void ServeData()
	{
		ModResult MOD_RESULT;
		std::string method = http_method_str(static_cast<http_method>(parser.method));
		HTTPRequestURI parsed;
		ParseURI(uri, parsed);
		HTTPRequest acl(method, parsed, &headers, this, ip, body);
		FIRST_MOD_RESULT_CUSTOM(*aclevprov, HTTPACLEventListener, OnHTTPACLCheck, MOD_RESULT, (acl));
		if (MOD_RESULT != MOD_RES_DENY)
		{
			HTTPRequest request(method, parsed, &headers, this, ip, body);
			FIRST_MOD_RESULT_CUSTOM(*reqevprov, HTTPRequestEventListener, OnHTTPRequest, MOD_RESULT, (request));
			if (MOD_RESULT == MOD_RES_PASSTHRU)
			{
				SendHTTPError(404);
			}
		}
	}

	void Page(const std::string& s, unsigned int response, HTTPHeaders* hheaders)
	{
		SendHeaders(s.length(), response, *hheaders);
		WriteData(s);
		Close(true);
	}

	void Page(std::stringstream* n, unsigned int response, HTTPHeaders* hheaders)
	{
		Page(n->str(), response, hheaders);
	}

	void AddToCull()
	{
		if (waitingcull)
			return;

		waitingcull = true;
		Close();
		ServerInstance->GlobalCulls.AddItem(this);
	}

	bool ParseURI(const std::string& uristr, HTTPRequestURI& out)
	{
		http_parser_url_init(&url);
		if (http_parser_parse_url(uristr.c_str(), uristr.size(), 0, &url) != 0)
			return false;

		if (url.field_set & (1 << UF_PATH))
			out.path = uri.substr(url.field_data[UF_PATH].off, url.field_data[UF_PATH].len);

		if (url.field_set & (1 << UF_FRAGMENT))
			out.fragment = uri.substr(url.field_data[UF_FRAGMENT].off, url.field_data[UF_FRAGMENT].len);

		std::string param_str;
		if (url.field_set & (1 << UF_QUERY))
			param_str = uri.substr(url.field_data[UF_QUERY].off, url.field_data[UF_QUERY].len);

		irc::sepstream param_stream(param_str, '&');
		std::string token;
		std::string::size_type eq_pos;
		while (param_stream.GetToken(token))
		{
			eq_pos = token.find('=');
			if (eq_pos == std::string::npos)
			{
				out.query_params.insert(std::make_pair(token, ""));
			}
			else
			{
				out.query_params.insert(std::make_pair(token.substr(0, eq_pos), token.substr(eq_pos + 1)));
			}
		}
		return true;
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
		HttpServerSocket::ConfigureParser();
	}

	void init() CXX11_OVERRIDE
	{
		HttpModule = this;
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("httpd");
		timeoutsec = tag->getDuration("timeout", 10, 1);
	}

	ModResult OnAcceptConnection(int nfd, ListenSocket* from, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server) CXX11_OVERRIDE
	{
		if (!stdalgo::string::equalsci(from->bind_tag->getString("type"), "httpd"))
			return MOD_RES_PASSTHRU;

		sockets.push_front(new HttpServerSocket(nfd, client->addr(), from, client, server, timeoutsec));
		return MOD_RES_ALLOW;
	}

	void OnUnloadModule(Module* mod) CXX11_OVERRIDE
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
