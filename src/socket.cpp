/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2005-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
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

int InspIRCd::BindPorts(FailedPortList &failed_ports)
{
	int bound = 0;
	std::vector<ListenSocket*> old_ports(ports.begin(), ports.end());

	ConfigTagList tags = ServerInstance->Config->ConfTags("bind");
	for(ConfigIter i = tags.first; i != tags.second; ++i)
	{
		ConfigTag* tag = i->second;
		std::string porttag = tag->getString("port");
		std::string Addr = tag->getString("address");

		if (strncasecmp(Addr.c_str(), "::ffff:", 7) == 0)
			this->Logs->Log("SOCKET", LOG_DEFAULT, "Using 4in6 (::ffff:) isn't recommended. You should bind IPv4 addresses directly instead.");

		irc::portparser portrange(porttag, false);
		int portno = -1;
		while (0 != (portno = portrange.GetToken()))
		{
			irc::sockets::sockaddrs bindspec;
			if (!irc::sockets::aptosa(Addr, portno, bindspec))
				continue;
			std::string bind_readable = bindspec.str();

			bool skip = false;
			for (std::vector<ListenSocket*>::iterator n = old_ports.begin(); n != old_ports.end(); ++n)
			{
				if ((**n).bind_desc == bind_readable)
				{
					(*n)->bind_tag = tag; // Replace tag, we know addr and port match, but other info (type, ssl) may not
					(*n)->ResetIOHookProvider();

					skip = true;
					old_ports.erase(n);
					break;
				}
			}
			if (!skip)
			{
				ListenSocket* ll = new ListenSocket(tag, bindspec);

				if (ll->GetFd() > -1)
				{
					bound++;
					ports.push_back(ll);
				}
				else
				{
					failed_ports.push_back(std::make_pair(bind_readable, strerror(errno)));
					delete ll;
				}
			}
		}
	}

	std::vector<ListenSocket*>::iterator n = ports.begin();
	for (std::vector<ListenSocket*>::iterator o = old_ports.begin(); o != old_ports.end(); ++o)
	{
		while (n != ports.end() && *n != *o)
			n++;
		if (n == ports.end())
		{
			this->Logs->Log("SOCKET", LOG_DEFAULT, "Port bindings slipped out of vector, aborting close!");
			break;
		}

		this->Logs->Log("SOCKET", LOG_DEFAULT, "Port binding %s was removed from the config file, closing.",
			(**n).bind_desc.c_str());
		delete *n;

		// this keeps the iterator valid, pointing to the next element
		n = ports.erase(n);
	}

	return bound;
}

bool irc::sockets::aptosa(const std::string& addr, int port, irc::sockets::sockaddrs& sa)
{
	memset(&sa, 0, sizeof(sa));
	if (addr.empty() || addr.c_str()[0] == '*')
	{
		if (ServerInstance->Config->WildcardIPv6)
		{
			sa.in6.sin6_family = AF_INET6;
			sa.in6.sin6_port = htons(port);
		}
		else
		{
			sa.in4.sin_family = AF_INET;
			sa.in4.sin_port = htons(port);
		}
		return true;
	}
	else if (inet_pton(AF_INET, addr.c_str(), &sa.in4.sin_addr) > 0)
	{
		sa.in4.sin_family = AF_INET;
		sa.in4.sin_port = htons(port);
		return true;
	}
	else if (inet_pton(AF_INET6, addr.c_str(), &sa.in6.sin6_addr) > 0)
	{
		sa.in6.sin6_family = AF_INET6;
		sa.in6.sin6_port = htons(port);
		return true;
	}
	return false;
}

int irc::sockets::sockaddrs::port() const
{
	if (sa.sa_family == AF_INET)
		return ntohs(in4.sin_port);
	if (sa.sa_family == AF_INET6)
		return ntohs(in6.sin6_port);
	return -1;
}

std::string irc::sockets::sockaddrs::addr() const
{
	char addrv[INET6_ADDRSTRLEN+1];
	if (sa.sa_family == AF_INET)
	{
		if (!inet_ntop(AF_INET, (void*)&in4.sin_addr, addrv, sizeof(addrv)))
			return "";
		return addrv;
	}
	else if (sa.sa_family == AF_INET6)
	{
		if (!inet_ntop(AF_INET6, (void*)&in6.sin6_addr, addrv, sizeof(addrv)))
			return "";
		return addrv;
	}
	return "";
}

bool irc::sockets::satoap(const irc::sockets::sockaddrs& sa, std::string& addr, int &port)
{
	port = sa.port();
	addr = sa.addr();
	return !addr.empty();
}

std::string irc::sockets::sockaddrs::str() const
{
	if (sa.sa_family == AF_INET)
	{
		char ipaddr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, (void*)&in4.sin_addr, ipaddr, sizeof(ipaddr));
		return InspIRCd::Format("%s:%u", ipaddr, ntohs(in4.sin_port));
	}

	if (sa.sa_family == AF_INET6)
	{
		char ipaddr[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, (void*)&in6.sin6_addr, ipaddr, sizeof(ipaddr));
		return InspIRCd::Format("[%s]:%u", ipaddr, ntohs(in6.sin6_port));
	}

	// This should never happen.
	return "<unknown>";
}

int irc::sockets::sockaddrs::sa_size() const
{
	if (sa.sa_family == AF_INET)
		return sizeof(in4);
	if (sa.sa_family == AF_INET6)
		return sizeof(in6);
	return 0;
}

bool irc::sockets::sockaddrs::operator==(const irc::sockets::sockaddrs& other) const
{
	if (sa.sa_family != other.sa.sa_family)
		return false;
	if (sa.sa_family == AF_INET)
		return (in4.sin_port == other.in4.sin_port) && (in4.sin_addr.s_addr == other.in4.sin_addr.s_addr);
	if (sa.sa_family == AF_INET6)
		return (in6.sin6_port == other.in6.sin6_port) && !memcmp(in6.sin6_addr.s6_addr, other.in6.sin6_addr.s6_addr, 16);
	return !memcmp(this, &other, sizeof(*this));
}

static void sa2cidr(irc::sockets::cidr_mask& cidr, const irc::sockets::sockaddrs& sa, int range)
{
	const unsigned char* base;
	unsigned char target_byte;
	cidr.type = sa.sa.sa_family;

	memset(cidr.bits, 0, sizeof(cidr.bits));

	if (cidr.type == AF_INET)
	{
		target_byte = sizeof(sa.in4.sin_addr);
		base = (unsigned char*)&sa.in4.sin_addr;
		if (range > 32)
			range = 32;
	}
	else if (cidr.type == AF_INET6)
	{
		target_byte = sizeof(sa.in6.sin6_addr);
		base = (unsigned char*)&sa.in6.sin6_addr;
		if (range > 128)
			range = 128;
	}
	else
	{
		cidr.length = 0;
		return;
	}
	cidr.length = range;
	unsigned int border = range / 8;
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

irc::sockets::cidr_mask::cidr_mask(const irc::sockets::sockaddrs& sa, int range)
{
	sa2cidr(*this, sa, range);
}

irc::sockets::cidr_mask::cidr_mask(const std::string& mask)
{
	std::string::size_type bits_chars = mask.rfind('/');
	irc::sockets::sockaddrs sa;

	if (bits_chars == std::string::npos)
	{
		irc::sockets::aptosa(mask, 0, sa);
		sa2cidr(*this, sa, 128);
	}
	else
	{
		int range = ConvToInt(mask.substr(bits_chars + 1));
		irc::sockets::aptosa(mask.substr(0, bits_chars), 0, sa);
		sa2cidr(*this, sa, range);
	}
}

std::string irc::sockets::cidr_mask::str() const
{
	irc::sockets::sockaddrs sa;
	sa.sa.sa_family = type;
	unsigned char* base;
	int len;
	if (type == AF_INET)
	{
		base = (unsigned char*)&sa.in4.sin_addr;
		len = 4;
	}
	else if (type == AF_INET6)
	{
		base = (unsigned char*)&sa.in6.sin6_addr;
		len = 16;
	}
	else
		return "";
	memcpy(base, bits, len);
	return sa.addr() + "/" + ConvToStr((int)length);
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
	if (addr.sa.sa_family != type)
		return false;
	irc::sockets::cidr_mask tmp(addr, length);
	return tmp == *this;
}
