/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2018 edef <edef@edef.eu>
 *   Copyright (C) 2013-2014, 2017, 2019-2023, 2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 John Brooks <john@jbrooks.io>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006, 2008 Craig Edwards <brain@inspircd.org>
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

/// $CompilerFlags: require_environment("SYSTEM_HTTP_PARSER" "1") -DUSE_SYSTEM_HTTP_PARSER
/// $LinkerFlags: require_environment("SYSTEM_HTTP_PARSER" "1") -lhttp_parser


#include "inspircd.h"
#include "iohook.h"
#include "modules/httpd.h"
#include "utility/string.h"

#ifdef __GNUC__
# pragma GCC diagnostic push
#endif

// Fix warnings about shadowing in http_parser.
#ifdef __GNUC__
# pragma GCC diagnostic ignored "-Wshadow"
#endif

#ifdef USE_SYSTEM_HTTP_PARSER
# include <http_parser.h>
#else
# include <http_parser/http_parser.c>
#endif

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
class HttpServerSocket final
	: public BufferedSocket
	, public Timer
	, public insp::intrusive_list_node<HttpServerSocket>
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
	int status_code = 0;

	/** True if this object is in the cull list
	 */
	bool waitingcull = false;
	bool messagecomplete = false;

	bool Tick() override
	{
		if (!messagecomplete)
		{
			ServerInstance->Logs.Debug(MODNAME, "HTTP socket {} timed out", GetFd());
			Close();
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
		parser_settings.on_header_value = DataCallback<&HttpServerSocket::OnHeaderValue>;
		parser_settings.on_headers_complete = Callback<&HttpServerSocket::OnHeadersComplete>;
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
	HttpServerSocket(int newfd, const std::string& IP, ListenSocket* via, const irc::sockets::sockaddrs& client, const irc::sockets::sockaddrs& server, unsigned long timeoutsec)
		: BufferedSocket(newfd)
		, Timer(timeoutsec, false)
		, ip(IP)
	{
		if ((!via->iohookprovs.empty()) && (via->iohookprovs.back()))
		{
			via->iohookprovs.back()->OnAccept(this, client, server);
			// IOHook may have errored
			if (!GetError().empty())
			{
				ServerInstance->Logs.Debug(MODNAME, "HTTP socket {} encountered a hook error: {}",
					GetFd(), GetError());
				Close();
				return;
			}
		}

		parser.data = this;
		http_parser_init(&parser, HTTP_REQUEST);
		ServerInstance->Timers.AddTimer(this);
	}

	~HttpServerSocket() override
	{
		sockets.erase(this);
	}

	void Close() override
	{
		if (waitingcull || !HasFd())
			return;

		waitingcull = true;
		BufferedSocket::Close();
		ServerInstance->GlobalCulls.AddItem(this);
	}

	void OnError(BufferedSocketError err) override
	{
		ServerInstance->Logs.Debug(MODNAME, "HTTP socket {} encountered an error: {} - {}",
			GetFd(), (int)err, GetError());
		Close();
	}

	void SendHTTPError(unsigned int response, const char* errstr = nullptr)
	{
		if (!errstr)
			errstr = http_status_str((http_status)response);

		ServerInstance->Logs.Debug(MODNAME, "Sending HTTP error {}: {}", response, errstr);
		static HTTPHeaders empty;
		std::string data = INSP_FORMAT(
			"<html><head></head><body style='font-family: sans-serif; text-align: center'>"
			"<h1 style='font-size: 48pt'>Error {}</h1><h2 style='font-size: 24pt'>{}</h2><hr>"
			"<small>Powered by <a href='https://www.inspircd.org'>InspIRCd</a></small></body></html>",
			response, errstr);

		Page(data, response, &empty);
	}

	void SendHeaders(unsigned long size, unsigned int response, HTTPHeaders& rheaders)
	{
		WriteData(INSP_FORMAT("HTTP/{}.{} {} {}\r\n", parser.http_major ? parser.http_major : 1, parser.http_major ? parser.http_minor : 1, response, http_status_str((http_status)response)));

		rheaders.CreateHeader("Date", Time::ToString(ServerInstance->Time(), Time::RFC_1123, true));
		rheaders.CreateHeader("Server", INSPIRCD_BRANCH);
		rheaders.SetHeader("Content-Length", ConvToStr(size));

		if (size)
			rheaders.CreateHeader("Content-Type", "text/html");
		else
			rheaders.RemoveHeader("Content-Type");

		/* Supporting Connection: keep-alive causes a whole world of hurt synchronizing timeouts,
		 * so remove it, its not essential for what we need.
		 */
		rheaders.SetHeader("Connection", "Close");

		WriteData(rheaders.GetFormattedHeaders());
		WriteData("\r\n");
	}

	void OnDataReady() override
	{
		if (parser.upgrade || HTTP_PARSER_ERRNO(&parser))
			return;
		http_parser_execute(&parser, &parser_settings, recvq.data(), recvq.size());
		if (parser.upgrade)
			SendHTTPError(status_code ? status_code : 400);
		else if (HTTP_PARSER_ERRNO(&parser))
			SendHTTPError(status_code ? status_code : 400, http_errno_description((http_errno)parser.http_errno));
	}

	void ServeData()
	{
		std::string method = http_method_str(static_cast<http_method>(parser.method));
		HTTPRequestURI parsed;
		ParseURI(uri, parsed);
		HTTPRequest acl(method, parsed, &headers, this, ip, body);
		ModResult res = aclevprov->FirstResult(&HTTPACLEventListener::OnHTTPACLCheck, acl);
		if (res != MOD_RES_DENY)
		{
			HTTPRequest request(method, parsed, &headers, this, ip, body);
			res = reqevprov->FirstResult(&HTTPRequestEventListener::OnHTTPRequest, request);
			if (res == MOD_RES_PASSTHRU)
			{
				SendHTTPError(404);
			}
		}
	}

	void Page(const std::string& s, unsigned int response, HTTPHeaders* hheaders)
	{
		SendHeaders(s.length(), response, *hheaders);
		WriteData(s);
		BufferedSocket::Close(true);
	}

	void Page(std::stringstream* n, unsigned int response, HTTPHeaders* hheaders)
	{
		Page(n->str(), response, hheaders);
	}

	bool ParseURI(const std::string& uristr, HTTPRequestURI& out)
	{
		http_parser_url_init(&url);
		if (http_parser_parse_url(uristr.c_str(), uristr.size(), 0, &url) != 0)
			return false;

		if (url.field_set & (1 << UF_PATH))
		{
			// Normalise the path.
			std::vector<std::string> pathsegments;
			irc::sepstream pathstream(uri.substr(url.field_data[UF_PATH].off, url.field_data[UF_PATH].len), '/');
			for (std::string pathsegment; pathstream.GetToken(pathsegment); )
			{
				if (pathsegment == ".")
				{
					// Stay at the current level.
					continue;
				}

				if (pathsegment == "..")
				{
					// Traverse up to the previous level.
					if (!pathsegments.empty())
						pathsegments.pop_back();
					continue;
				}

				pathsegments.push_back(pathsegment);
			}

			out.path.reserve(url.field_data[UF_PATH].len);
			out.path.append("/").append(insp::join(pathsegments, '/'));
		}

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
				out.query_params.emplace(token, "");
			}
			else
			{
				out.query_params.emplace(token.substr(0, eq_pos), token.substr(eq_pos + 1));
			}
		}
		return true;
	}
};

class HTTPdAPIImpl final
	: public HTTPdAPIBase
{
public:
	HTTPdAPIImpl(Module* parent)
		: HTTPdAPIBase(parent)
	{
	}

	void SendResponse(HTTPDocumentResponse& resp) override
	{
		resp.src.sock->Page(resp.document, resp.responsecode, &resp.headers);
	}
};

class ModuleHttpServer final
	: public Module
{
private:
	HTTPdAPIImpl APIImpl;
	unsigned long timeoutsec;
	Events::ModuleEventProvider acleventprov;
	Events::ModuleEventProvider reqeventprov;

public:
	ModuleHttpServer()
		: Module(VF_VENDOR, "Allows the server administrator to serve various useful resources over HTTP.")
		, APIImpl(this)
		, acleventprov(this, "event/http-acl")
		, reqeventprov(this, "event/http-request")
	{
		aclevprov = &acleventprov;
		reqevprov = &reqeventprov;
		HttpServerSocket::ConfigureParser();
	}

	void init() override
	{
		HttpModule = this;
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("httpd");
		timeoutsec = tag->getDuration("timeout", 10, 1);
	}

	ModResult OnAcceptConnection(int nfd, ListenSocket* from, const irc::sockets::sockaddrs& client, const irc::sockets::sockaddrs& server) override
	{
		if (!insp::equalsci(from->bind_tag->getString("type"), "httpd"))
			return MOD_RES_PASSTHRU;

		sockets.push_front(new HttpServerSocket(nfd, client.addr(), from, client, server, timeoutsec));
		return MOD_RES_ALLOW;
	}

	void OnUnloadModule(Module* mod) override
	{
		for (insp::intrusive_list<HttpServerSocket>::const_iterator i = sockets.begin(); i != sockets.end(); )
		{
			HttpServerSocket* sock = *i;
			++i;
			if (sock->GetModHook(mod))
			{
				sock->Cull();
				delete sock;
			}
		}
	}

	Cullable::Result Cull() override
	{
		for (auto* sock : sockets)
			sock->Close();
		return Module::Cull();
	}
};

MODULE_INIT(ModuleHttpServer)
