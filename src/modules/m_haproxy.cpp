/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2018-2019, 2021, 2023 Sadie Powell <sadie@witchery.services>
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
#include "modules/ssl.h"

enum
{
	// The SSL TLV flag for a client being connected over SSL.
	PP2_CLIENT_SSL = 0x01,

	// The family for TCP over IPv4.
	PP2_FAMILY_IPV4 = 0x11,

	// The length of the PP2_FAMILY_IPV4 endpoints.
	PP2_FAMILY_IPV4_LENGTH = 12,

	// The family for TCP over IPv6.
	PP2_FAMILY_IPV6 = 0x21,

	// The length of the PP2_FAMILY_IPV6 endpoints.
	PP2_FAMILY_IPV6_LENGTH = 36,

	// The family for UNIX sockets.
	PP2_FAMILY_UNIX = 0x31,

	// The length of the PP2_FAMILY_UNIX endpoints.
	PP2_FAMILY_UNIX_LENGTH = 216,

	// The bitmask we apply to extract the command.
	PP2_COMMAND_MASK = 0x0F,

	// The length of the PROXY protocol header.
	PP2_HEADER_LENGTH = 16,

	// The minimum length of a Type-Length-Value entry.
	PP2_TLV_LENGTH = 3,

	// The identifier for a SSL TLV entry.
	PP2_TYPE_SSL = 0x20,

	// The minimum length of a PP2_TYPE_SSL TLV entry.
	PP2_TYPE_SSL_LENGTH = 5,

	// The length of the PROXY protocol signature.
	PP2_SIGNATURE_LENGTH = 12,

	// The PROXY protocol version we support.
	PP2_VERSION = 0x20,

	// The bitmask we apply to extract the protocol version.
	PP2_VERSION_MASK = 0xF0
};

enum HAProxyState
{
	// We are waiting for the PROXY header section.
	HPS_WAITING_FOR_HEADER,

	// We are waiting for the PROXY address section.
	HPS_WAITING_FOR_ADDRESS,

	// The client is fully connected.
	HPS_CONNECTED
};

enum HAProxyCommand
{
	// LOCAL command.
	HPC_LOCAL = 0x00,

	// PROXY command.
	HPC_PROXY = 0x01
};

struct HAProxyHeader
{
	// The signature used to identify the HAProxy protocol.
	uint8_t signature[PP2_SIGNATURE_LENGTH];

	// The version of the PROXY protocol and command being sent.
	uint8_t version_command;

	// The family for the address.
	uint8_t family;

	// The length of the address section.
	uint16_t length;
};

class HAProxyHookProvider : public IOHookProvider
{
 private:
	UserCertificateAPI sslapi;

 public:
	HAProxyHookProvider(Module* mod)
		: IOHookProvider(mod, "haproxy", IOHookProvider::IOH_UNKNOWN, true)
		, sslapi(mod)
	{
	}

	void OnAccept(StreamSocket* sock, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server) CXX11_OVERRIDE;

	void OnConnect(StreamSocket* sock) CXX11_OVERRIDE
	{
		// We don't need to implement this.
	}
};

// The signature for a HAProxy PROXY protocol header.
static const char proxy_signature[13] = "\x0D\x0A\x0D\x0A\x00\x0D\x0A\x51\x55\x49\x54\x0A";

class HAProxyHook : public IOHookMiddle
{
 private:
	// The length of the address section.
	uint16_t address_length;

	// The endpoint the client is connecting from.
	irc::sockets::sockaddrs client;

	// The command sent by the proxy server.
	HAProxyCommand command;

	// The endpoint the client is connected to.
	irc::sockets::sockaddrs server;

	// The API for interacting with user SSL internals.
	UserCertificateAPI& sslapi;

	// The current state of the PROXY parser.
	HAProxyState state;

	size_t ReadProxyTLV(StreamSocket* sock, size_t start_index, uint16_t buffer_length)
	{
		// A TLV must at least consist of a type (uint8_t) and a length (uint16_t).
		if (buffer_length < PP2_TLV_LENGTH)
		{
			sock->SetError("Truncated HAProxy PROXY TLV type and/or length");
			return 0;
		}

		// Check that the length can actually contain the TLV value.
		std::string& recvq = GetRecvQ();
		uint16_t length = ntohs(recvq[start_index + 1] | (recvq[start_index + 2] << 8));
		if (buffer_length < PP2_TLV_LENGTH + length)
		{
			sock->SetError("Truncated HAProxy PROXY TLV value");
			return 0;
		}

		// What type of TLV are we parsing?
		switch (recvq[start_index])
		{
			case PP2_TYPE_SSL:
				if (!ReadProxyTLVSSL(sock, start_index + PP2_TLV_LENGTH, length))
					return 0;
				break;
		}

		return PP2_TLV_LENGTH + length;
	}

	bool ReadProxyTLVSSL(StreamSocket* sock, size_t start_index, uint16_t buffer_length)
	{
		// A SSL TLV must at least consist of client info (uint8_t) and verification info (uint32_t).
		if (buffer_length < PP2_TYPE_SSL_LENGTH)
		{
			sock->SetError("Truncated HAProxy PROXY SSL TLV");
			return false;
		}

		// If the socket is not a user socket we don't have to do
		// anything with this TLVs information.
		if (sock->type != StreamSocket::SS_USER)
			return true;

		// If the sslinfo module is not loaded we can't
		// do anything with this TLV.
		if (!sslapi)
			return true;

		// If the client is not connecting via TLS (SSL) the rest of this TLV is irrelevant.
		std::string& recvq = GetRecvQ();
		if ((recvq[start_index] & PP2_CLIENT_SSL) == 0)
			return true;

		// Create a fake ssl_cert for the user. Ideally we should use the user's
		// TLS (SSL) client certificate here but as of 2018-10-16 this is not forwarded
		// by HAProxy.
		ssl_cert* cert = new ssl_cert;
		cert->error = "HAProxy does not forward client TLS (SSL) certificates";
		cert->invalid = true;
		cert->revoked = true;
		cert->trusted = false;
		cert->unknownsigner = true;

		// Extract the user for this socket and set their certificate.
		LocalUser* luser = static_cast<UserIOHandler*>(sock)->user;
		sslapi->SetCertificate(luser, cert);
		return true;
	}

	int ReadData(std::string& destrecvq)
	{
		// Once connected we handle no special data.
		std::string& recvq = GetRecvQ();
		destrecvq.append(recvq);
		recvq.clear();
		return 1;
	}

	int ReadProxyAddress(StreamSocket* sock, std::string& destrecvq)
	{
		// Block until we have the entire address.
		std::string& recvq = GetRecvQ();
		if (recvq.length() < address_length)
			return 0;

		switch (command)
		{
			case HPC_LOCAL:
				// Skip the address completely.
				recvq.erase(0, address_length);
				break;

			case HPC_PROXY:
				// Store the endpoint information.
				size_t tlv_index = 0;
				switch (client.family())
				{
					case AF_INET:
						memcpy(&client.in4.sin_addr.s_addr, &recvq[0], 4);
						memcpy(&server.in4.sin_addr.s_addr, &recvq[4], 4);
						memcpy(&client.in4.sin_port, &recvq[8], 2);
						memcpy(&server.in4.sin_port, &recvq[10], 2);
						tlv_index = 12;
						break;

					case AF_INET6:
						memcpy(client.in6.sin6_addr.s6_addr, &recvq[0], 16);
						memcpy(server.in6.sin6_addr.s6_addr, &recvq[16], 16);
						memcpy(&client.in6.sin6_port, &recvq[32], 2);
						memcpy(&server.in6.sin6_port, &recvq[34], 2);
						tlv_index = 36;
						break;

					case AF_UNIX:
						memcpy(client.un.sun_path, &recvq[0], 108);
						memcpy(server.un.sun_path, &recvq[108], 108);
						tlv_index = 216;
						break;
				}

				if (!sock->OnSetEndPoint(server, client))
					return -1;

				// Parse any available TLVs.
				while (tlv_index < address_length)
				{
					size_t length = ReadProxyTLV(sock, tlv_index, address_length - tlv_index);
					if (!length)
						return -1;

					tlv_index += length;
				}

				// Erase the processed proxy information from the receive queue.
				recvq.erase(0, address_length);
				break;
		}

		// We're done!
		state = HPS_CONNECTED;
		return ReadData(destrecvq);
	}

	int ReadProxyHeader(StreamSocket* sock, std::string& destrecvq)
	{
		// Block until we have a header.
		std::string& recvq = GetRecvQ();
		if (recvq.length() < PP2_HEADER_LENGTH)
			return 0;

		// Read the header.
		HAProxyHeader header;
		memcpy(&header, recvq.c_str(), PP2_HEADER_LENGTH);
		recvq.erase(0, PP2_HEADER_LENGTH);

		// Check we are actually parsing a HAProxy header.
		if (memcmp(&header.signature, proxy_signature, PP2_SIGNATURE_LENGTH) != 0)
		{
			// If we've reached this point the proxy server did not send a proxy information.
			sock->SetError("Invalid HAProxy PROXY signature");
			return -1;
		}

		// We only support this version of the protocol.
		const uint8_t version = (header.version_command & PP2_VERSION_MASK);
		if (version != PP2_VERSION)
		{
			sock->SetError("Unsupported HAProxy PROXY protocol version");
			return -1;
		}

		// We only support the LOCAL and PROXY commands.
		command = static_cast<HAProxyCommand>(header.version_command & PP2_COMMAND_MASK);
		switch (command)
		{
			case HPC_LOCAL:
				// Intentionally left blank.
				break;

			case HPC_PROXY:
				// Check the protocol support and initialise the sockaddrs.
				uint16_t shortest_length;
				switch (header.family)
				{
					case PP2_FAMILY_IPV4: // TCP over IPv4.
						client.sa.sa_family = server.sa.sa_family = AF_INET;
						shortest_length = PP2_FAMILY_IPV4_LENGTH;
						break;

					case PP2_FAMILY_IPV6: // TCP over IPv6.
						client.sa.sa_family = server.sa.sa_family = AF_INET6;
						shortest_length = PP2_FAMILY_IPV6_LENGTH;
						break;

					case PP2_FAMILY_UNIX: // UNIX stream.
						client.sa.sa_family = server.sa.sa_family = AF_UNIX;
						shortest_length = PP2_FAMILY_UNIX_LENGTH;
						break;

					default: // Unknown protocol.
						sock->SetError("Invalid HAProxy PROXY protocol type");
						return -1;
				}

				// Check that the length can actually contain the addresses.
				address_length = ntohs(header.length);
				if (address_length < shortest_length)
				{
					sock->SetError("Truncated HAProxy PROXY address section");
					return -1;
				}
				break;

			default:
				sock->SetError("Unsupported HAProxy PROXY command");
				return -1;
		}

		state = HPS_WAITING_FOR_ADDRESS;
		return ReadProxyAddress(sock, destrecvq);
	}

 public:
	HAProxyHook(IOHookProvider* Prov, StreamSocket* sock, UserCertificateAPI& api)
		: IOHookMiddle(Prov)
		, address_length(0)
		, sslapi(api)
		, state(HPS_WAITING_FOR_HEADER)
	{
		sock->AddIOHook(this);
	}

	bool IsHookReady() const CXX11_OVERRIDE
	{
		return state == HPS_CONNECTED;
	}

	int OnStreamSocketWrite(StreamSocket* sock, StreamSocket::SendQueue& uppersendq) CXX11_OVERRIDE
	{
		// We don't need to implement this.
		GetSendQ().moveall(uppersendq);
		return 1;
	}

	int OnStreamSocketRead(StreamSocket* sock, std::string& destrecvq) CXX11_OVERRIDE
	{
		switch (state)
		{
			case HPS_WAITING_FOR_HEADER:
				return ReadProxyHeader(sock, destrecvq);

			case HPS_WAITING_FOR_ADDRESS:
				return ReadProxyAddress(sock, destrecvq);

			case HPS_CONNECTED:
				return ReadData(destrecvq);
		}

		// We should never reach this point.
		return -1;
	}

	void OnStreamSocketClose(StreamSocket* sock) CXX11_OVERRIDE
	{
		// We don't need to implement this.
	}
};

void HAProxyHookProvider::OnAccept(StreamSocket* sock, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server)
{
	new HAProxyHook(this, sock, sslapi);
}

class ModuleHAProxy : public Module
{
 private:
	reference<HAProxyHookProvider> hookprov;

 public:
	ModuleHAProxy()
		: hookprov(new HAProxyHookProvider(this))
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Allows IRC connections to be made using reverse proxies that implement version 2 of the HAProxy PROXY protocol.", VF_VENDOR);
	}
};

MODULE_INIT(ModuleHAProxy)
