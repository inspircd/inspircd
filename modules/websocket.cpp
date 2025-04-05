/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2019 iwalkalone <iwalkalone69@gmail.com>
 *   Copyright (C) 2018-2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2016 Attila Molnar <attilamolnar@hush.com>
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
#include "extension.h"
#include "iohook.h"
#include "modules/hash.h"
#include "modules/isupport.h"
#include "modules/whois.h"
#include "utility/string.h"

#define UTF_CPP_CPLUSPLUS 199711L
#include <utfcpp/unchecked.h>

static constexpr char MagicGUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
static constexpr char newline[] = "\r\n";
static constexpr char whitespace[] = " \t";

struct SharedData final
{
	// An extension that stores the value of the Origin header.
	StringExtItem origin;

	// An extension that stores the real hostname for proxied hosts.
	StringExtItem realhost;

	// An extension that stores the real IP for proxied hosts.
	StringExtItem realip;

	// A reference to the SHA-1 provider.
	Hash::ProviderRef sha1;

	SharedData(Module* mod)
		: origin(mod, "websocket-origin", ExtensionType::USER, true)
		, realhost(mod, "websocket-realhost", ExtensionType::USER, true)
		, realip(mod, "websocket-realip", ExtensionType::USER, true)
		, sha1(mod, "sha1")
	{
	}
};

static SharedData* g_data;

struct WebSocketConfig final
{
	enum DefaultMode
	{
		// Reject connections if a subprotocol is not requested.
		DM_REJECT,

		// Use binary frames if a subprotocol is not requested.
		DM_BINARY,

		// Use UTF-8 text frames if a subprotocol is not requested.
		DM_TEXT
	};

	typedef std::vector<std::string> OriginList;
	typedef std::vector<std::string> ProxyRanges;

	// The HTTP origins that can connect to the server.
	OriginList allowedorigins;

	// Whether text encoding can be used on this server.
	bool allowtext;

	// The method to use if a subprotocol is not negotiated.
	DefaultMode defaultmode;

	// The IP ranges which send trustworthy X-Real-IP or X-Forwarded-For headers.
	ProxyRanges proxyranges;

	// Whether to allow connections from clients that do not send an Origin header.
	bool allowmissingorigin;

	// Whether to send WebSocket ping messages instead of IRC ping messages.
	bool nativeping;
};

class WebSocketHookProvider final
	: public IOHookProvider
{
public:
	WebSocketConfig config;
	WebSocketHookProvider(Module* mod)
		: IOHookProvider(mod, "websocket", IOHookProvider::IOH_UNKNOWN, true)
	{
	}

	void OnAccept(StreamSocket* sock, const irc::sockets::sockaddrs& client, const irc::sockets::sockaddrs& server) override;

	void OnConnect(StreamSocket* sock) override
	{
	}
};

class WebSocketHook final
	: public IOHookMiddle
{
	class HTTPHeaderFinder final
	{
	private:
		std::string::size_type bpos;
		std::string::size_type len;

	public:
		bool Find(const std::string& req, const char* header, std::string::size_type headerlen, std::string::size_type maxpos)
		{
			// Skip the GET /wibble HTTP/1.1 line.
			size_t startpos = req.find(newline) + sizeof(newline) - 1;
			while (startpos < maxpos)
			{
				size_t endpos = req.find(newline, startpos);
				if (strncasecmp(req.c_str() + startpos, header, headerlen) != 0)
				{
					startpos = endpos + sizeof(newline) - 1;
					continue; // Incorrect header.
				}

				bpos = req.find_first_not_of(whitespace, startpos + headerlen);
				if (bpos >= endpos)
					return false; // Empty value.

				size_t epos = std::min(req.find_last_not_of(whitespace), endpos);
				if (epos < bpos)
					return false; // Should never happen.

				len = epos - bpos;
				return true;
			}

			// No header found.
			return false;
		}

		std::string ExtractValue(const std::string& req) const
		{
			return std::string(req, bpos, len);
		}
	};

	enum CloseCode
	{
		CLOSE_PROTOCOL_ERROR = 1002,
		CLOSE_POLICY_VIOLATION = 1008,
		CLOSE_TOO_LARGE = 1009
	};

	enum OpCode
	{
		OP_CONTINUATION = 0x00,
		OP_TEXT = 0x01,
		OP_BINARY = 0x02,
		OP_CLOSE = 0x08,
		OP_PING = 0x09,
		OP_PONG = 0x0a
	};

	enum State
	{
		STATE_HTTPREQ,
		STATE_ESTABLISHED
	};

	static constexpr unsigned char WS_MASKBIT = (1 << 7);
	static constexpr unsigned char WS_FINBIT = (1 << 7);
	static constexpr unsigned char WS_PAYLOAD_LENGTH_MAGIC_LARGE = 126;
	static constexpr unsigned char WS_PAYLOAD_LENGTH_MAGIC_HUGE = 127;
	static constexpr size_t WS_MAX_PAYLOAD_LENGTH_SMALL = 125;
	static constexpr size_t WS_MAX_PAYLOAD_LENGTH_LARGE = 65535;
	static constexpr size_t MAXHEADERSIZE = sizeof(uint64_t) + 2;

	// Clients sending ping or pong frames faster than this are killed
	static constexpr time_t MINPINGPONGDELAY = 10;

	State state = STATE_HTTPREQ;
	time_t lastpingpong = 0;
	WebSocketConfig& config;
	bool sendastext;

	static size_t FillHeader(unsigned char* outbuf, size_t sendlength, OpCode opcode)
	{
		size_t pos = 0;
		outbuf[pos++] = WS_FINBIT | opcode;

		if (sendlength <= WS_MAX_PAYLOAD_LENGTH_SMALL)
		{
			outbuf[pos++] = sendlength;
		}
		else if (sendlength <= WS_MAX_PAYLOAD_LENGTH_LARGE)
		{
			outbuf[pos++] = WS_PAYLOAD_LENGTH_MAGIC_LARGE;
			outbuf[pos++] = (sendlength >> 8) & 0xff;
			outbuf[pos++] = sendlength & 0xff;
		}
		else
		{
			outbuf[pos++] = WS_PAYLOAD_LENGTH_MAGIC_HUGE;
			const uint64_t len = sendlength;
			for (int i = sizeof(uint64_t)-1; i >= 0; i--)
				outbuf[pos++] = ((len >> i*8) & 0xff);
		}

		return pos;
	}

	static StreamSocket::SendQueue::Element PrepareSendQElem(size_t size, OpCode opcode)
	{
		unsigned char header[MAXHEADERSIZE];
		const size_t n = FillHeader(header, size, opcode);

		return StreamSocket::SendQueue::Element(reinterpret_cast<const char*>(header), n);
	}

	int HandleAppData(StreamSocket* sock, std::string& appdataout, bool allowlarge)
	{
		std::string& myrecvq = GetRecvQ();
		// Need 1 byte opcode, minimum 1 byte len, 4 bytes masking key
		if (myrecvq.length() < 6)
			return 0;

		const std::string& cmyrecvq = myrecvq;
		unsigned char len1 = (unsigned char)cmyrecvq[1];
		if (!(len1 & WS_MASKBIT))
		{
			CloseConnection(sock, CLOSE_PROTOCOL_ERROR, "WebSocket protocol violation: unmasked client frame");
			return -1;
		}

		len1 &= ~WS_MASKBIT;

		// Assume the length is a single byte, if not, update values later
		unsigned int len = len1;
		unsigned int payloadstartoffset = 6;
		const unsigned char* maskkey = reinterpret_cast<const unsigned char*>(&cmyrecvq[2]);

		if (len1 == WS_PAYLOAD_LENGTH_MAGIC_LARGE)
		{
			// allowlarge is false for control frames according to the RFC meaning large pings, etc. are not allowed
			if (!allowlarge)
			{
				CloseConnection(sock, CLOSE_PROTOCOL_ERROR, "WebSocket protocol violation: large control frame");
				return -1;
			}

			// Large frame, has 2 bytes len after the magic byte indicating the length
			// Need 1 byte opcode, 3 bytes len, 4 bytes masking key
			if (myrecvq.length() < 8)
				return 0;

			unsigned char len2 = (unsigned char)cmyrecvq[2];
			unsigned char len3 = (unsigned char)cmyrecvq[3];
			len = (len2 << 8) | len3;

			if (len <= WS_MAX_PAYLOAD_LENGTH_SMALL)
			{
				CloseConnection(sock, CLOSE_PROTOCOL_ERROR, "WebSocket protocol violation: non-minimal length encoding used");
				return -1;
			}

			maskkey += 2;
			payloadstartoffset += 2;
		}
		else if (len1 == WS_PAYLOAD_LENGTH_MAGIC_HUGE)
		{
			CloseConnection(sock, CLOSE_TOO_LARGE, "WebSocket: Huge frames are not supported");
			return -1;
		}

		if (myrecvq.length() < payloadstartoffset + len)
			return 0;

		unsigned int maskkeypos = 0;
		const std::string::iterator endit = myrecvq.begin() + payloadstartoffset + len;
		for (std::string::const_iterator i = myrecvq.begin() + payloadstartoffset; i != endit; ++i)
		{
			const unsigned char c = (unsigned char)*i;
			appdataout.push_back(c ^ maskkey[maskkeypos++]);
			maskkeypos %= 4;
		}

		myrecvq.erase(myrecvq.begin(), endit);
		return 1;
	}

	int HandlePingPongFrame(StreamSocket* sock, bool isping)
	{
		if (lastpingpong + MINPINGPONGDELAY >= ServerInstance->Time())
		{
			CloseConnection(sock, CLOSE_POLICY_VIOLATION, "WebSocket: Ping/pong flood");
			return -1;
		}

		lastpingpong = ServerInstance->Time();

		std::string appdata;
		const int result = HandleAppData(sock, appdata, false);
		if (result <= 0)
			return result;

		if (isping)
		{
			StreamSocket::SendQueue::Element elem = PrepareSendQElem(appdata.length(), OP_PONG);
			elem.append(appdata);
			GetSendQ().push_back(elem);

			SocketEngine::ChangeEventMask(sock, FD_ADD_TRIAL_WRITE);
		}
		else if (sock->type == StreamSocket::SS_USER && config.nativeping)
		{
			// Pong reply on user socket; reset their idle time.
			UserIOHandler* ioh = static_cast<UserIOHandler*>(sock);
			ioh->user->lastping = 1;
		}
		return 1;
	}

	int HandleWS(StreamSocket* sock, std::string& destrecvq)
	{
		if (GetRecvQ().empty())
			return 0;

		unsigned char opcode = (unsigned char)GetRecvQ()[0];
		switch (opcode & ~WS_FINBIT)
		{
			case OP_CONTINUATION:
			case OP_TEXT:
			case OP_BINARY:
			{
				std::string appdata;
				const int result = HandleAppData(sock, appdata, true);
				if (result != 1)
					return result;

				// Strip out any CR+LF which may have been erroneously sent.
				for (const auto chr : appdata)
				{
					if (chr != '\r' && chr != '\n')
						destrecvq.push_back(chr);
				}

				// If we are on the final message of this block append a line terminator.
				if (opcode & WS_FINBIT)
					destrecvq.append("\r\n");

				return 1;
			}

			case OP_PING:
			{
				return HandlePingPongFrame(sock, true);
			}

			case OP_PONG:
			{
				// A pong frame may be sent unsolicited, so we have to handle it.
				// It may carry application data which we need to remove from the recvq as well.
				return HandlePingPongFrame(sock, false);
			}

			case OP_CLOSE:
			{
				sock->SetError("Connection closed");
				return -1;
			}

			default:
			{
				CloseConnection(sock, CLOSE_PROTOCOL_ERROR, "WebSocket: Invalid opcode");
				return -1;
			}
		}
	}

	void CloseConnection(StreamSocket* sock, CloseCode closecode, const std::string& reason)
	{
		uint16_t netcode = htons(closecode);
		std::string packedcode;
		packedcode.push_back(netcode & 0x00FF);
		packedcode.push_back(netcode >> 8);

		GetSendQ().push_back(PrepareSendQElem(reason.length() + 2, OP_CLOSE));
		GetSendQ().push_back(packedcode);
		GetSendQ().push_back(reason);
		sock->DoWrite();
		sock->SetError(reason);
	}

	void FailHandshake(StreamSocket* sock, const char* httpreply, const char* sockerror)
	{
		GetSendQ().push_back(StreamSocket::SendQueue::Element(httpreply));
		GetSendQ().push_back(StreamSocket::SendQueue::Element(sockerror));
		sock->DoWrite();
		sock->SetError(sockerror);
	}

	int HandleHTTPReq(StreamSocket* sock)
	{
		std::string& recvq = GetRecvQ();
		const std::string::size_type reqend = recvq.find("\r\n\r\n");
		if (reqend == std::string::npos)
			return 0;

		LocalUser* luser = nullptr;
		if (sock->type == StreamSocket::SS_USER)
			luser = static_cast<UserIOHandler*>(sock)->user;

		bool allowedorigin = false;
		HTTPHeaderFinder originheader;
		if (originheader.Find(recvq, "Origin:", 7, reqend))
		{
			const std::string origin = originheader.ExtractValue(recvq);
			for (const auto& cfgorigin : config.allowedorigins)
			{
				if (InspIRCd::Match(origin, cfgorigin, ascii_case_insensitive_map))
				{
					allowedorigin = true;
					if (luser)
						g_data->origin.Set(luser, origin);
					break;
				}
			}
		}
		else if (config.allowmissingorigin)
		{
			// This is a non-web WebSocket connection.
			allowedorigin = true;
		}
		else
		{
			FailHandshake(sock, "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n", "WebSocket: Received HTTP request that did not send the Origin header");
			return -1;
		}

		if (!allowedorigin)
		{
			FailHandshake(sock, "HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\n", "WebSocket: Received HTTP request from a non-whitelisted origin");
			return -1;
		}

		if (!config.proxyranges.empty() && luser)
		{
			irc::sockets::sockaddrs realsa(luser->client_sa);

			HTTPHeaderFinder proxyheader;
			if (proxyheader.Find(recvq, "X-Real-IP:", 10, reqend) || proxyheader.Find(recvq, "X-Forwarded-For:", 16, reqend))
			{
				// Attempt to parse the proxy HTTP header.
				if (!realsa.from_ip_port(proxyheader.ExtractValue(recvq), realsa.port()))
				{
					// The proxy header value contains a malformed value.
					FailHandshake(sock, "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n", "WebSocket: Received a proxied HTTP request that sent a malformed real IP address");
					return -1;
				}
			}
			else
			{
				// The proxy header is missing.
				FailHandshake(sock, "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n", "WebSocket: Received a proxied HTTP request that did not send a real IP address header");
				return -1;
			}
			for (const auto& proxyrange : config.proxyranges)
			{
				if (InspIRCd::MatchCIDR(luser->GetAddress(), proxyrange, ascii_case_insensitive_map))
				{
					// Give the user their real IP address.
					if (realsa != luser->client_sa)
					{
						g_data->realhost.Set(luser, luser->GetRealHost());
						g_data->realip.Set(luser, luser->GetAddress());
						luser->ChangeRemoteAddress(realsa);
					}

					// Error if changing their IP gets them banned.
					if (luser->quitting)
						return -1;
					break;
				}
			}
		}

		std::string selectedproto;
		HTTPHeaderFinder protocolheader;
		if (protocolheader.Find(recvq, "Sec-WebSocket-Protocol:", 23, reqend))
		{
			irc::commasepstream protostream(protocolheader.ExtractValue(recvq));
			for (std::string proto; protostream.GetToken(proto); )
			{
				std::erase_if(proto, ::isspace);

				auto is_binary = (sock->type == StreamSocket::SS_USER && insp::equalsci(proto, "binary.ircv3.net"))
					|| insp::equalsci(proto, "binary.inspircd.org");

				auto is_text = (sock->type == StreamSocket::SS_USER && insp::equalsci(proto, "text.ircv3.net"))
					|| insp::equalsci(proto, "text.inspircd.org");

				if (is_binary || is_text)
				{
					selectedproto = std::move(proto);
					sendastext = is_text;
					break;
				}
			}
		}

		if (selectedproto.empty() && config.defaultmode == WebSocketConfig::DM_REJECT)
		{
			FailHandshake(sock, "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n", "WebSocket: Received HTTP request that did not send the Sec-WebSocket-Protocol header");
			return -1;
		}

		if (sendastext && !config.allowtext)
		{
			FailHandshake(sock, "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n", "WebSocket: Received HTTP request that requested a text protocol which is not compatible with the server character set");
			return -1;
		}

		HTTPHeaderFinder keyheader;
		if (!keyheader.Find(recvq, "Sec-WebSocket-Key:", 18, reqend))
		{
			FailHandshake(sock, "HTTP/1.1 501 Not Implemented\r\nConnection: close\r\n\r\n", "WebSocket: Received HTTP request which is not a websocket upgrade");
			return -1;
		}

		if (!*g_data->sha1)
		{
			FailHandshake(sock, "HTTP/1.1 503 Service Unavailable\r\nConnection: close\r\n\r\n", "WebSocket: SHA-1 provider missing");
			return -1;
		}

		state = STATE_ESTABLISHED;

		std::string key = keyheader.ExtractValue(recvq);
		key.append(MagicGUID);

		std::string reply = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ";
		reply.append(Base64::Encode((*g_data->sha1)->Hash(key), nullptr, '=')).append(newline);
		if (!selectedproto.empty())
			reply.append("Sec-WebSocket-Protocol: ").append(selectedproto).append(newline);
		reply.append(newline);
		GetSendQ().push_back(StreamSocket::SendQueue::Element(reply));

		SocketEngine::ChangeEventMask(sock, FD_ADD_TRIAL_WRITE);

		recvq.erase(0, reqend + 4);

		return 1;
	}

public:
	WebSocketHook(const std::shared_ptr<IOHookProvider>& Prov, StreamSocket* sock, WebSocketConfig& cfg)
		: IOHookMiddle(Prov)
		, config(cfg)
		, sendastext(config.defaultmode != WebSocketConfig::DM_BINARY)
	{
		sock->AddIOHook(this);
	}

	bool IsHookReady() const override
	{
		return state == STATE_ESTABLISHED;
	}

	ssize_t OnStreamSocketWrite(StreamSocket* sock, StreamSocket::SendQueue& uppersendq) override
	{
		StreamSocket::SendQueue& mysendq = GetSendQ();

		// Return 1 to allow sending back an error HTTP response
		if (state != STATE_ESTABLISHED)
			return (mysendq.empty() ? 0 : 1);

		std::string message;
		for (const auto& elem : uppersendq)
		{
			for (const auto chr : elem)
			{
				if (chr == '\n')
				{
					// We have found an entire message. Send it in its own frame.
					if (sendastext)
					{
						// If we send messages as text then we need to ensure they are valid UTF-8.
						std::string encoded;
						utf8::unchecked::replace_invalid(message.begin(), message.end(), std::back_inserter(encoded));

						mysendq.push_back(PrepareSendQElem(encoded.length(), OP_TEXT));
						mysendq.push_back(encoded);
					}
					else
					{
						// Otherwise, send the raw message as a binary frame.
						mysendq.push_back(PrepareSendQElem(message.length(), OP_BINARY));
						mysendq.push_back(message);
					}
					message.clear();
				}
				else if (chr != '\r')
				{
					message.push_back(chr);
				}
			}
		}

		// Empty the upper send queue and push whatever is left back onto it.
		uppersendq.clear();
		if (!message.empty())
		{
			uppersendq.push_back(message);
			return 0;
		}

		return 1;
	}

	ssize_t OnStreamSocketRead(StreamSocket* sock, std::string& destrecvq) override
	{
		if (state == STATE_HTTPREQ)
		{
			int httpret = HandleHTTPReq(sock);
			if (httpret <= 0)
				return httpret;
		}

		int wsret;
		do
		{
			wsret = HandleWS(sock, destrecvq);
		}
		while ((!GetRecvQ().empty()) && (wsret > 0));

		return wsret;
	}

	bool Ping() override
	{
		if (!config.nativeping)
			return false;

		StreamSocket::SendQueue& mysendq = GetSendQ();

		const std::string& message = ServerInstance->Config->GetServerName();
		mysendq.push_back(PrepareSendQElem(message.length(), OP_PING));
		mysendq.push_back(message);

		return true;
	}
};

void WebSocketHookProvider::OnAccept(StreamSocket* sock, const irc::sockets::sockaddrs& client, const irc::sockets::sockaddrs& server)
{
	new WebSocketHook(shared_from_this(), sock, config);
}

class ModuleWebSocket final
	: public Module
	, public Whois::EventListener
{
private:
	std::shared_ptr<WebSocketHookProvider> hookprov;
	ISupport::EventProvider isupportprov;
	SharedData data;

public:
	ModuleWebSocket()
		: Module(VF_VENDOR, "Allows WebSocket clients to connect to the IRC server.")
		, Whois::EventListener(this)
		, hookprov(std::make_shared<WebSocketHookProvider>(this))
		, isupportprov(this)
		, data(this)
	{
		g_data = &data;
	}

	void ReadConfig(ConfigStatus& status) override
	{
		auto tags = ServerInstance->Config->ConfTags("wsorigin");
		if (tags.empty())
			throw ModuleException(this, "You have loaded the websocket module but not configured any allowed origins!");

		WebSocketConfig config;

		// If a server has a non-utf8 compatible character set configured
		// then we can not support text encoding.
		ISupport::TokenMap tokens;
		isupportprov.Call(&ISupport::EventListener::OnBuildISupport, tokens);
		auto it = tokens.find("CHARSET");
		config.allowtext = it == tokens.end()
			|| irc::equals(it->second, "ascii")
			|| irc::equals(it->second, "utf8");

		for (const auto& [_, tag] : tags)
		{
			// Ensure that we have the <wsorigin:allow> parameter.
			const std::string allow = tag->getString("allow");
			if (allow.empty())
				throw ModuleException(this, "<wsorigin:allow> is a mandatory field, at " + tag->source.str());

			config.allowedorigins.push_back(allow);
		}

		const auto& tag = ServerInstance->Config->ConfValue("websocket");

		const std::string defaultmodestr = tag->getString("defaultmode", config.allowtext ? "text" : "binary", 1);
		if (insp::equalsci(defaultmodestr, "reject"))
			config.defaultmode = WebSocketConfig::DM_REJECT;
		else if (insp::equalsci(defaultmodestr, "binary"))
			config.defaultmode = WebSocketConfig::DM_BINARY;
		else if (insp::equalsci(defaultmodestr, "text"))
			config.defaultmode = WebSocketConfig::DM_TEXT;
		else
			throw ModuleException(this, defaultmodestr + " is an invalid value for <websocket:defaultmode>; acceptable values are 'binary', 'text' and 'reject', at " + tag->source.str());

		if (config.defaultmode == WebSocketConfig::DM_TEXT && !config.allowtext)
			throw ModuleException(this, "You can not use text websockets when using a non-utf8 compatible server charset, at " + tag->source.str());

		irc::spacesepstream proxyranges(tag->getString("proxyranges"));
		for (std::string proxyrange; proxyranges.GetToken(proxyrange); )
			config.proxyranges.push_back(proxyrange);

		config.allowmissingorigin = tag->getBool("allowmissingorigin", true);
		config.nativeping = tag->getBool("nativeping", true);

		// Everything is okay; apply the new config.
		hookprov->config = config;
	}

	void OnCleanup(ExtensionType type, Extensible* item) override
	{
		if (type != ExtensionType::USER)
			return;

		LocalUser* user = IS_LOCAL(static_cast<User*>(item));
		if ((user) && (user->eh.GetModHook(this)))
			ServerInstance->Users.QuitUser(user, "WebSocket module unloading");
	}

	void OnWhois(Whois::Context& whois) override
	{
		if (!whois.GetSource()->HasPrivPermission("users/auspex"))
			return; // Only visible to opers.

		const auto* origin = data.origin.Get(whois.GetTarget());
		const auto* realhost = data.realhost.Get(whois.GetTarget());
		const auto* realip = data.realip.Get(whois.GetTarget());
		if (!origin && !realhost && !realip)
			return; // If these fields are not set then the client is not a WebSocket user.

		// If either of these aren't set then the user's connection isn't proxied.
		std::string missing = "*";
		if (!realhost || !realip)
			realhost = realip = &missing;

		if (origin)
			whois.SendLine(RPL_WHOISGATEWAY, *realhost, *realip, "is connected from the " + *origin + " WebSocket origin");
		else
			whois.SendLine(RPL_WHOISGATEWAY, *realhost, *realip, "is connected from a non-web WebSocket origin");
	}
};

MODULE_INIT(ModuleWebSocket)
