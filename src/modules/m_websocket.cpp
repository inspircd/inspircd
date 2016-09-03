/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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
#include "iohook.h"
#include "modules/hash.h"

static const char MagicGUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
static const char whitespace[] = " \t\r\n";
static dynamic_reference_nocheck<HashProvider>* sha1;

class WebSocketHookProvider : public IOHookProvider
{
 public:
	WebSocketHookProvider(Module* mod)
		: IOHookProvider(mod, "websocket", IOHookProvider::IOH_UNKNOWN, true)
	{
	}

	void OnAccept(StreamSocket* sock, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server) CXX11_OVERRIDE;

	void OnConnect(StreamSocket* sock) CXX11_OVERRIDE
	{
	}
};

class WebSocketHook : public IOHookMiddle
{
	class HTTPHeaderFinder
	{
		std::string::size_type bpos;
		std::string::size_type len;

	 public:
		bool Find(const std::string& req, const char* header, std::string::size_type headerlen, std::string::size_type maxpos)
		{
			std::string::size_type keybegin = req.find(header);
			if ((keybegin == std::string::npos) || (keybegin > maxpos) || (keybegin == 0) || (req[keybegin-1] != '\n'))
				return false;

			keybegin += headerlen;

			bpos = req.find_first_not_of(whitespace, keybegin, sizeof(whitespace)-1);
			if ((bpos == std::string::npos) || (bpos > maxpos))
				return false;

			const std::string::size_type epos = req.find_first_of(whitespace, bpos, sizeof(whitespace)-1);
			len = epos - bpos;

			return true;
		}

		std::string ExtractValue(const std::string& req) const
		{
			return std::string(req, bpos, len);
		}
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

	static const unsigned char WS_MASKBIT = (1 << 7);
	static const unsigned char WS_FINBIT = (1 << 7);
	static const unsigned char WS_PAYLOAD_LENGTH_MAGIC_LARGE = 126;
	static const unsigned char WS_PAYLOAD_LENGTH_MAGIC_HUGE = 127;
	static const size_t WS_MAX_PAYLOAD_LENGTH_SMALL = 125;
	static const size_t WS_MAX_PAYLOAD_LENGTH_LARGE = 65535;
	static const size_t MAXHEADERSIZE = sizeof(uint64_t) + 2;

	// Clients sending ping or pong frames faster than this are killed
	static const time_t MINPINGPONGDELAY = 10;

	State state;
	time_t lastpingpong;

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
			sock->SetError("WebSocket protocol violation: unmasked client frame");
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
				sock->SetError("WebSocket protocol violation: large control frame");
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
				sock->SetError("WebSocket protocol violation: non-minimal length encoding used");
				return -1;
			}

			maskkey += 2;
			payloadstartoffset += 2;
		}
		else if (len1 == WS_PAYLOAD_LENGTH_MAGIC_HUGE)
		{
			sock->SetError("WebSocket: Huge frames are not supported");
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
			sock->SetError("WebSocket: Ping/pong flood");
			return -1;
		}

		lastpingpong = ServerInstance->Time();

		std::string appdata;
		const int result = HandleAppData(sock, appdata, false);
		// If it's a pong stop here regardless of the result so we won't generate a reply
		if ((result <= 0) || (!isping))
			return result;

		StreamSocket::SendQueue::Element elem = PrepareSendQElem(appdata.length(), OP_PONG);
		elem.append(appdata);
		GetSendQ().push_back(elem);

		SocketEngine::ChangeEventMask(sock, FD_ADD_TRIAL_WRITE);
		return 1;
	}

	int HandleWS(StreamSocket* sock, std::string& destrecvq)
	{
		if (GetRecvQ().empty())
			return 0;

		unsigned char opcode = (unsigned char)GetRecvQ().c_str()[0];
		opcode &= ~WS_FINBIT;

		switch (opcode)
		{
			case OP_CONTINUATION:
			case OP_TEXT:
			case OP_BINARY:
			{
				return HandleAppData(sock, destrecvq, true);
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
				sock->SetError("WebSocket: Invalid opcode");
				return -1;
			}
		}
	}

	void FailHandshake(StreamSocket* sock, const char* httpreply, const char* sockerror)
	{
		GetSendQ().push_back(StreamSocket::SendQueue::Element(httpreply));
		sock->DoWrite();
		sock->SetError(sockerror);
	}

	int HandleHTTPReq(StreamSocket* sock)
	{
		std::string& recvq = GetRecvQ();
		const std::string::size_type reqend = recvq.find("\r\n\r\n");
		if (reqend == std::string::npos)
			return 0;

		HTTPHeaderFinder keyheader;
		if (!keyheader.Find(recvq, "Sec-WebSocket-Key:", 18, reqend))
		{
			FailHandshake(sock, "HTTP/1.1 501 Not Implemented\r\nConnection: close\r\n\r\n", "WebSocket: Received HTTP request which is not a websocket upgrade");
			return -1;
		}

		if (!*sha1)
		{
			FailHandshake(sock, "HTTP/1.1 503 Service Unavailable\r\nConnection: close\r\n\r\n", "WebSocket: SHA-1 provider missing");
			return -1;
		}

		state = STATE_ESTABLISHED;

		std::string key = keyheader.ExtractValue(recvq);
		key.append(MagicGUID);

		std::string reply = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ";
		reply.append(BinToBase64((*sha1)->GenerateRaw(key), NULL, '=')).append("\r\n\r\n");
		GetSendQ().push_back(StreamSocket::SendQueue::Element(reply));

		SocketEngine::ChangeEventMask(sock, FD_ADD_TRIAL_WRITE);

		recvq.erase(0, reqend + 4);

		return 1;
	}

 public:
	WebSocketHook(IOHookProvider* Prov, StreamSocket* sock)
		: IOHookMiddle(Prov)
		, state(STATE_HTTPREQ)
		, lastpingpong(0)
	{
		sock->AddIOHook(this);
	}

	int OnStreamSocketWrite(StreamSocket* sock, StreamSocket::SendQueue& uppersendq) CXX11_OVERRIDE
	{
		StreamSocket::SendQueue& mysendq = GetSendQ();

		// Return 1 to allow sending back an error HTTP response
		if (state != STATE_ESTABLISHED)
			return (mysendq.empty() ? 0 : 1);

		if (!uppersendq.empty())
		{
			StreamSocket::SendQueue::Element elem = PrepareSendQElem(uppersendq.bytes(), OP_BINARY);
			mysendq.push_back(elem);
			mysendq.moveall(uppersendq);
		}

		return 1;
	}

	int OnStreamSocketRead(StreamSocket* sock, std::string& destrecvq) CXX11_OVERRIDE
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

	void OnStreamSocketClose(StreamSocket* sock) CXX11_OVERRIDE
	{
	}
};

void WebSocketHookProvider::OnAccept(StreamSocket* sock, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server)
{
	new WebSocketHook(this, sock);
}

class ModuleWebSocket : public Module
{
	dynamic_reference_nocheck<HashProvider> hash;
	WebSocketHookProvider hookprov;

 public:
	ModuleWebSocket()
		: hash(this, "hash/sha1")
		, hookprov(this)
	{
		sha1 = &hash;
	}

	void OnCleanup(int target_type, void* item) CXX11_OVERRIDE
	{
		if (target_type != TYPE_USER)
			return;

		LocalUser* user = IS_LOCAL(static_cast<User*>(item));
		if ((user) && (user->eh.GetModHook(this)))
			ServerInstance->Users.QuitUser(user, "WebSocket module unloading");
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides RFC 6455 WebSocket support", VF_VENDOR);
	}
};

MODULE_INIT(ModuleWebSocket)
