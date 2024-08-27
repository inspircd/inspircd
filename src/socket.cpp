/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2017-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2011 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@gmail.com>
 *   Copyright (C) 2007 John Brooks <john@jbrooks.io>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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


#ifndef _WIN32
# include <arpa/inet.h>
# include <netinet/in.h>
#endif

#include "inspircd.h"
#include "utility/string.h"

namespace
{
	// Checks whether the system can create SCTP sockets.
	bool CanCreateSCTPSocket()
	{
#ifdef IPPROTO_SCTP
		int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
		if (fd >= 0)
		{
			SocketEngine::Close(fd);
			return true;
		}
#endif
		return false;
	}
}

bool InspIRCd::BindPort(const std::shared_ptr<ConfigTag>& tag, const irc::sockets::sockaddrs& sa, std::vector<ListenSocket*>& old_ports, sa_family_t protocol)
{
	for (std::vector<ListenSocket*>::iterator n = old_ports.begin(); n != old_ports.end(); ++n)
	{
		ListenSocket* ls = *n;
		if (ls->bind_sa == sa && protocol == ls->bind_protocol)
		{
			// Replace tag, we know addr and port match, but other info (type, ssl) may not.
			ServerInstance->Logs.Debug("SOCKET", "Replacing listener on {} from old tag at {} with new tag from {}",
				sa.str(), ls->bind_tag->source.str(), tag->source.str());
			ls->bind_tag = tag;
			ls->ResetIOHookProvider();

			old_ports.erase(n);
			return true;
		}
	}

	auto* ll = new ListenSocket(tag, sa, protocol);
	if (!ll->HasFd())
	{
		ServerInstance->Logs.Normal("SOCKET", "Failed to listen on {} from tag at {}: {}",
			sa.str(), tag->source.str(), SocketEngine::LastError());
		delete ll;
		return false;
	}

	ServerInstance->Logs.Debug("SOCKET", "Added a listener on {} from tag at {}", sa.str(), tag->source.str());
	ports.push_back(ll);
	return true;
}

size_t InspIRCd::BindPorts(FailedPortList& failed_ports)
{
	size_t bound = 0;
	std::vector<ListenSocket*> old_ports(ports.begin(), ports.end());

	for (const auto& [_, tag] : ServerInstance->Config->ConfTags("bind"))
	{
		// Are we creating a TCP/SCTP listener?
		const std::string address = tag->getString("address");
		const std::string portlist = tag->getString("port");
		if (!address.empty() || !portlist.empty())
		{
			// InspIRCd supports IPv4 and IPv6 natively; no 4in6 required.
			if (strncasecmp(address.c_str(), "::ffff:", 7) == 0)
				this->Logs.Warning("SOCKET", "Using 4in6 (::ffff:) isn't recommended. You should bind IPv4 addresses directly instead.");

			// Try to parse the bind address.
			irc::sockets::sockaddrs bindspec(true);
			if (!bindspec.from_ip(address))
			{
				failed_ports.emplace_back("Address is not valid: " + address, tag);
				continue;
			}

			// A TCP listener with no ports is not very useful.
			if (portlist.empty())
			{
				failed_ports.emplace_back("No ports specified", bindspec, tag);
				continue;
			}

			irc::portparser portrange(portlist, false);
			while (long port = portrange.GetToken())
			{
				// Check if the port is out of range.
				if (port <= std::numeric_limits<in_port_t>::min() || port > std::numeric_limits<in_port_t>::max())
				{
					failed_ports.emplace_back("Port is not valid: " + ConvToStr(port), bindspec, tag);
					continue;
				}

				switch (bindspec.family())
				{
					case AF_INET:
						bindspec.in4.sin_port = htons(static_cast<in_port_t>(port));
						break;

					case AF_INET6:
						bindspec.in6.sin6_port = htons(static_cast<in_port_t>(port));
						break;

					default:
						continue; // Should never happen.
				}

				std::vector<int> protocols;
				irc::spacesepstream protostream(tag->getString("protocols", "all", 1));
				for (std::string protocol; protostream.GetToken(protocol); )
				{
					if (insp::equalsci(protocol, "all"))
					{
						protocols.push_back(0); // IPPROTO_TCP
#ifdef IPPROTO_SCTP
						if (CanCreateSCTPSocket())
							protocols.push_back(IPPROTO_SCTP);
#endif
					}
					else if (insp::equalsci(protocol, "sctp"))
					{
#ifdef IPPROTO_SCTP
						protocols.push_back(IPPROTO_SCTP);
#else
						failed_ports.emplace_back("Platform does not support SCTP", tag);
#endif
					}
					else if (insp::equalsci(protocol, "tcp"))
					{
						protocols.push_back(0); // IPPROTO_TCP
					}
					else
					{
						failed_ports.emplace_back("Protocol is not valid: " + protocol, tag);
					}
				}

				for (const auto protocol : protocols)
				{
					if (!BindPort(tag, bindspec, old_ports, protocol))
						failed_ports.emplace_back(SocketEngine::LastError(), bindspec, tag);
					else
						bound++;
				}
			}
			continue;
		}

		// Are we creating a UNIX listener?
		const std::string path = tag->getString("path");
		if (!path.empty())
		{
			// Expand the path relative to the config directory.
			const std::string fullpath = ServerInstance->Config->Paths.PrependRuntime(path);

			// UNIX socket paths are length limited to less than PATH_MAX.
			irc::sockets::sockaddrs bindspec;
			if (fullpath.length() > std::min(ServerInstance->Config->Limits.MaxHost, sizeof(bindspec.un.sun_path) - 1))
			{
				failed_ports.emplace_back("Path is too long: " + fullpath, tag);
				continue;
			}

			// Check for characters which are problematic in the IRC message format.
			if (fullpath.find_first_of("\n\r\t!@: ") != std::string::npos)
			{
				failed_ports.emplace_back("Path contains invalid characters: " + fullpath, tag);
				continue;
			}

			bindspec.from_unix(fullpath);
			if (!BindPort(tag, bindspec, old_ports, 0))
				failed_ports.emplace_back(SocketEngine::LastError(), bindspec, tag);
			else
				bound++;
		}
	}

	std::vector<ListenSocket*>::iterator n = ports.begin();
	for (auto* old_port : old_ports)
	{
		while (n != ports.end() && *n != old_port)
			n++;
		if (n == ports.end())
		{
			this->Logs.Warning("SOCKET", "Port bindings slipped out of vector, aborting close!");
			break;
		}

		this->Logs.Debug("SOCKET", "Port binding {} was removed from the config file, closing.",
			(**n).bind_sa.str());
		delete *n;

		// this keeps the iterator valid, pointing to the next element
		n = ports.erase(n);
	}

	return bound;
}

irc::sockets::sockaddrs::sockaddrs(bool initialize)
{
	if (initialize)
		memset(this, 0, sizeof(*this));
}

bool irc::sockets::sockaddrs::from_ip_port(const std::string& addr, in_port_t port)
{
	if (addr.empty() || addr == "*")
	{
		if (ServerInstance->Config->WildcardIPv6)
		{
			memset(&in6.sin6_addr, 0, sizeof(in6.sin6_addr));
			in6.sin6_family = AF_INET6;
			in6.sin6_port = htons(port);
		}
		else
		{
			memset(&in4.sin_addr, 0, sizeof(in4.sin_addr));
			in4.sin_family = AF_INET;
			in4.sin_port = htons(port);
		}
		return true;
	}
	else if (inet_pton(AF_INET, addr.c_str(), &in4.sin_addr) > 0)
	{
		in4.sin_family = AF_INET;
		in4.sin_port = htons(port);
		return true;
	}
	else if (inet_pton(AF_INET6, addr.c_str(), &in6.sin6_addr) > 0)
	{
		in6.sin6_family = AF_INET6;
		in6.sin6_port = htons(port);
		return true;
	}
	return false;
}

bool irc::sockets::sockaddrs::from_unix(const std::string& path)
{
	if (path.length() >= sizeof(un.sun_path))
		return false;

	un.sun_family = AF_UNIX;
	memcpy(&un.sun_path, path.c_str(), path.length() + 1);
	return true;
}

bool irc::sockets::isunix(const std::string& file)
{
#ifndef _WIN32
	struct stat sb;
	if (stat(file.c_str(), &sb) == 0 && S_ISSOCK(sb.st_mode))
		return true;
#endif
	return false;
}

sa_family_t irc::sockets::sockaddrs::family() const
{
	return sa.sa_family;
}

bool irc::sockets::sockaddrs::is_local() const
{
	switch (family())
	{
		case AF_INET:
			return irc::sockets::cidr_mask("127.0.0.0/8").match(*this);

		case AF_INET6:
			return irc::sockets::cidr_mask("::1/128").match(*this);

		case AF_UNIX:
			return true;
	}

	// If we have reached this point then we have encountered a bug.
	ServerInstance->Logs.Debug("SOCKET", "BUG: irc::sockets::sockaddrs::is_local(): socket type {} is unknown!", family());
	return false;
}

in_port_t irc::sockets::sockaddrs::port() const
{
	switch (family())
	{
		case AF_INET:
			return ntohs(in4.sin_port);

		case AF_INET6:
			return ntohs(in6.sin6_port);

		case AF_UNIX:
			return 0;
	}

	// If we have reached this point then we have encountered a bug.
	ServerInstance->Logs.Debug("SOCKET", "BUG: irc::sockets::sockaddrs::port(): socket type {} is unknown!", family());
	return 0;
}

std::string irc::sockets::sockaddrs::addr() const
{
	switch (family())
	{
		case AF_INET:
			char ip4addr[INET_ADDRSTRLEN];
			if (!inet_ntop(AF_INET, static_cast<const void*>(&in4.sin_addr), ip4addr, sizeof(ip4addr)))
				return "0.0.0.0";
			return ip4addr;

		case AF_INET6:
			char ip6addr[INET6_ADDRSTRLEN];
			if (!inet_ntop(AF_INET6, static_cast<const void*>(&in6.sin6_addr), ip6addr, sizeof(ip6addr)))
				return "0:0:0:0:0:0:0:0";
			return ip6addr;

		case AF_UNIX:
			return un.sun_path;
	}

	// If we have reached this point then we have encountered a bug.
	ServerInstance->Logs.Debug("SOCKET", "BUG: irc::sockets::sockaddrs::addr(): socket type {} is unknown!", family());
	return "<unknown>";
}

std::string irc::sockets::sockaddrs::str() const
{
	switch (family())
	{
		case AF_INET:
			char ip4addr[INET_ADDRSTRLEN];
			if (!inet_ntop(AF_INET, static_cast<const void*>(&in4.sin_addr), ip4addr, sizeof(ip4addr)))
				strcpy(ip4addr, "0.0.0.0");
			return INSP_FORMAT("{}:{}", ip4addr, ntohs(in4.sin_port));

		case AF_INET6:
			char ip6addr[INET6_ADDRSTRLEN];
			if (!inet_ntop(AF_INET6, static_cast<const void*>(&in6.sin6_addr), ip6addr, sizeof(ip6addr)))
				strcpy(ip6addr, "0:0:0:0:0:0:0:0");
			return INSP_FORMAT("[{}]:{}", ip6addr, ntohs(in6.sin6_port));

		case AF_UNIX:
			return un.sun_path;
	}

	// If we have reached this point then we have encountered a bug.
	ServerInstance->Logs.Debug("SOCKET", "BUG: irc::sockets::sockaddrs::str(): socket type {} is unknown!", family());
	return "<unknown>";
}

socklen_t irc::sockets::sockaddrs::sa_size() const
{
	switch (family())
	{
		case AF_INET:
			return sizeof(in4);

		case AF_INET6:
			return sizeof(in6);

		case AF_UNIX:
			return sizeof(un);
	}

	// If we have reached this point then we have encountered a bug.
	ServerInstance->Logs.Debug("SOCKET", "BUG: irc::sockets::sockaddrs::sa_size(): socket type {} is unknown!", family());
	return 0;
}

bool irc::sockets::sockaddrs::operator==(const irc::sockets::sockaddrs& other) const
{
	if (family() != other.family())
		return false;

	switch (family())
	{
		case AF_INET:
			return (in4.sin_port == other.in4.sin_port) && (in4.sin_addr.s_addr == other.in4.sin_addr.s_addr);

		case AF_INET6:
			return (in6.sin6_port == other.in6.sin6_port) && !memcmp(in6.sin6_addr.s6_addr, other.in6.sin6_addr.s6_addr, 16);

		case AF_UNIX:
			return !strcmp(un.sun_path, other.un.sun_path);
	}

	// If we have reached this point then we have encountered a bug.
	ServerInstance->Logs.Debug("SOCKET", "BUG: irc::sockets::sockaddrs::operator==(): socket type {} is unknown!", family());
	return !memcmp(this, &other, sizeof(*this));
}

static void sa2cidr(irc::sockets::cidr_mask& cidr, const irc::sockets::sockaddrs& sa, unsigned char range)
{
	const unsigned char* base;
	unsigned char target_byte;

	memset(cidr.bits, 0, sizeof(cidr.bits));

	cidr.type = sa.family();
	switch (cidr.type)
	{
		case AF_UNIX:
			// XXX: UNIX sockets don't support CIDR. This fix is non-ideal but I can't
			// really think of another way to handle it.
			cidr.length = 0;
			return;

		case AF_INET:
			cidr.length = range > 32 ? 32 : range;
			target_byte = sizeof(sa.in4.sin_addr);
			base = reinterpret_cast<const unsigned char*>(&sa.in4.sin_addr);
			break;

		case AF_INET6:
			cidr.length = range > 128 ? 128 : range;
			target_byte = sizeof(sa.in6.sin6_addr);
			base = reinterpret_cast<const unsigned char*>(&sa.in6.sin6_addr);
			break;

		default:
			// If we have reached this point then we have encountered a bug.
			ServerInstance->Logs.Debug("SOCKET", "BUG: sa2cidr(): socket type {} is unknown!", cidr.type);
			cidr.length = 0;
			return;
	}

	unsigned int border = cidr.length / 8;
	unsigned int bitmask = (0xFF00 >> (range & 7)) & 0xFF;
	for(unsigned int i=0; i < target_byte; i++)
	{
		if (i < border)
			cidr.bits[i] = base[i];
		else if (i == border)
			cidr.bits[i] = base[i] & bitmask;
		else
			return;
	}
}

irc::sockets::cidr_mask::cidr_mask(const irc::sockets::sockaddrs& sa, unsigned char range)
{
	sa2cidr(*this, sa, range);
}

irc::sockets::cidr_mask::cidr_mask(const std::string& mask)
{
	std::string::size_type bits_chars = mask.rfind('/');
	irc::sockets::sockaddrs sa;

	if (bits_chars == std::string::npos)
	{
		if (!sa.from_ip(mask))
		{
			memset(this, 0, sizeof(*this));
			return;
		}
		sa2cidr(*this, sa, 128);
	}
	else
	{
		unsigned char range = ConvToNum<unsigned char>(mask.substr(bits_chars + 1));
		if (!sa.from_ip(mask.substr(0, bits_chars)))
		{
			memset(this, 0, sizeof(*this));
			return;
		}
		sa2cidr(*this, sa, range);
	}
}

std::string irc::sockets::cidr_mask::str() const
{
	irc::sockets::sockaddrs sa;
	sa.sa.sa_family = type;

	unsigned char* base;
	size_t len;
	unsigned char maxlen;
	switch (type)
	{
		case AF_INET:
			base = reinterpret_cast<unsigned char*>(&sa.in4.sin_addr);
			len = 4;
			maxlen = 32;
			break;

		case AF_INET6:
			base = reinterpret_cast<unsigned char*>(&sa.in6.sin6_addr);
			len = 16;
			maxlen = 128;
			break;

		case AF_UNIX:
			// TODO: make bits a vector<uint8_t> so we can return the actual path here.
			return "/*";

		default:
			// If we have reached this point then we have encountered a bug.
			ServerInstance->Logs.Debug("SOCKET", "BUG: irc::sockets::cidr_mask::str(): socket type {} is unknown!", type);
			return "<unknown>";
	}

	memcpy(base, bits, len);

	std::string value = sa.addr();
	if (length < maxlen)
	{
		value.push_back('/');
		value.append(ConvToStr(static_cast<uint16_t>(length)));
	}
	return value;
}

bool irc::sockets::cidr_mask::operator==(const cidr_mask& other) const
{
	return type == other.type && length == other.length &&
		0 == memcmp(bits, other.bits, 16);
}

bool irc::sockets::cidr_mask::operator<(const cidr_mask& other) const
{
	if (type != other.type)
		return type < other.type;
	if (length != other.length)
		return length < other.length;
	return memcmp(bits, other.bits, 16) < 0;
}

bool irc::sockets::cidr_mask::match(const irc::sockets::sockaddrs& addr) const
{
	if (addr.family() != type)
		return false;
	irc::sockets::cidr_mask tmp(addr, length);
	return tmp == *this;
}
