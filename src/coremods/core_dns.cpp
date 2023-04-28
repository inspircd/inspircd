/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2015, 2017-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2015-2016, 2021 Adam <Adam@anope.org>
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
#include "modules/dns.h"
#include <iostream>
#include <fstream>

#ifdef _WIN32
#include <Iphlpapi.h>
#pragma comment(lib, "Iphlpapi.lib")
#endif

namespace DNS
{
	/** Maximum value of a dns request id, 16 bits wide, 0xFFFF.
	 */
	const unsigned int MAX_REQUEST_ID = 0xFFFF;
}

using namespace DNS;

/** A full packet sent or received to/from the nameserver
 */
class Packet : public Query
{
	void PackName(unsigned char* output, unsigned short output_size, unsigned short& pos, const std::string& name)
	{
		if (pos + name.length() + 2 > output_size)
			throw Exception("Unable to pack name");

		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Packing name " + name);

		irc::sepstream sep(name, '.');
		std::string token;

		while (sep.GetToken(token))
		{
			output[pos++] = token.length();
			memcpy(&output[pos], token.data(), token.length());
			pos += token.length();
		}

		output[pos++] = 0;
	}

	std::string UnpackName(const unsigned char* input, unsigned short input_size, unsigned short& pos)
	{
		std::string name;
		unsigned short pos_ptr = pos, lowest_ptr = input_size;
		bool compressed = false;

		if (pos_ptr >= input_size)
			throw Exception("Unable to unpack name - no input");

		while (input[pos_ptr] > 0)
		{
			unsigned short offset = input[pos_ptr];

			if (offset & POINTER)
			{
				if ((offset & POINTER) != POINTER)
					throw Exception("Unable to unpack name - bogus compression header");
				if (pos_ptr + 1 >= input_size)
					throw Exception("Unable to unpack name - bogus compression header");

				/* Place pos at the second byte of the first (farthest) compression pointer */
				if (compressed == false)
				{
					++pos;
					compressed = true;
				}

				pos_ptr = (offset & LABEL) << 8 | input[pos_ptr + 1];

				/* Pointers can only go back */
				if (pos_ptr >= lowest_ptr)
					throw Exception("Unable to unpack name - bogus compression pointer");
				lowest_ptr = pos_ptr;
			}
			else
			{
				if (pos_ptr + offset + 1 >= input_size)
					throw Exception("Unable to unpack name - offset too large");
				if (!name.empty())
					name += ".";
				for (unsigned i = 1; i <= offset; ++i)
					name += input[pos_ptr + i];

				pos_ptr += offset + 1;
				if (compressed == false)
					/* Move up pos */
					pos = pos_ptr;
			}
		}

		/* +1 pos either to one byte after the compression pointer or one byte after the ending \0 */
		++pos;

		if (name.empty())
			throw Exception("Unable to unpack name - no name");

		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Unpack name " + name);

		return name;
	}

	Question UnpackQuestion(const unsigned char* input, unsigned short input_size, unsigned short& pos)
	{
		Question q;

		q.name = this->UnpackName(input, input_size, pos);

		if (pos + 4 > input_size)
			throw Exception("Unable to unpack question");

		q.type = static_cast<QueryType>(input[pos] << 8 | input[pos + 1]);
		pos += 2;

		// Skip over query class code
		pos += 2;

		return q;
	}

	ResourceRecord UnpackResourceRecord(const unsigned char* input, unsigned short input_size, unsigned short& pos)
	{
		ResourceRecord record = static_cast<ResourceRecord>(this->UnpackQuestion(input, input_size, pos));

		if (pos + 6 > input_size)
			throw Exception("Unable to unpack resource record");

		record.ttl = (input[pos] << 24) | (input[pos + 1] << 16) | (input[pos + 2] << 8) | input[pos + 3];
		pos += 4;

		uint16_t rdlength = input[pos] << 8 | input[pos + 1];
		pos += 2;

		switch (record.type)
		{
			case QUERY_A:
			{
				if (pos + 4 > input_size)
					throw Exception("Unable to unpack resource record");

				irc::sockets::sockaddrs addrs;
				memset(&addrs, 0, sizeof(addrs));

				addrs.in4.sin_family = AF_INET;
				addrs.in4.sin_addr.s_addr = input[pos] | (input[pos + 1] << 8) | (input[pos + 2] << 16)  | (input[pos + 3] << 24);
				pos += 4;

				record.rdata = addrs.addr();
				break;
			}
			case QUERY_AAAA:
			{
				if (pos + 16 > input_size)
					throw Exception("Unable to unpack resource record");

				irc::sockets::sockaddrs addrs;
				memset(&addrs, 0, sizeof(addrs));

				addrs.in6.sin6_family = AF_INET6;
				for (int j = 0; j < 16; ++j)
					addrs.in6.sin6_addr.s6_addr[j] = input[pos + j];
				pos += 16;

				record.rdata = addrs.addr();

				break;
			}
			case QUERY_CNAME:
			case QUERY_PTR:
			{
				record.rdata = this->UnpackName(input, input_size, pos);
				if (!InspIRCd::IsHost2(record.rdata, true))
					throw Exception("Invalid name"); // XXX: Causes the request to time out

				break;
			}
			case QUERY_TXT:
			{
				if (pos + rdlength > input_size)
					throw Exception("Unable to unpack txt resource record");

				record.rdata = std::string(reinterpret_cast<const char *>(input + pos), rdlength);
				pos += rdlength;

				if (record.rdata.find_first_of("\r\n\0", 0, 3) != std::string::npos)
					throw Exception("Invalid character in txt record");

				break;
			}
			default:
			{
				if (pos + rdlength > input_size)
					throw Exception("Unable to skip resource record");

				pos += rdlength;
				break;
			}
		}

		if (!record.name.empty() && !record.rdata.empty())
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, record.name + " -> " + record.rdata);

		return record;
	}

 public:
	static const int POINTER = 0xC0;
	static const int LABEL = 0x3F;
	static const int HEADER_LENGTH = 12;

	/* ID for this packet */
	RequestId id;
	/* Flags on the packet */
	unsigned short flags;

	Packet() : id(0), flags(0)
	{
	}

	void Fill(const unsigned char* input, const unsigned short len)
	{
		if (len < HEADER_LENGTH)
			throw Exception("Unable to fill packet");

		unsigned short packet_pos = 0;

		this->id = (input[packet_pos] << 8) | input[packet_pos + 1];
		packet_pos += 2;

		this->flags = (input[packet_pos] << 8) | input[packet_pos + 1];
		packet_pos += 2;

		unsigned short qdcount = (input[packet_pos] << 8) | input[packet_pos + 1];
		packet_pos += 2;

		unsigned short ancount = (input[packet_pos] << 8) | input[packet_pos + 1];
		packet_pos += 2;

		unsigned short nscount = (input[packet_pos] << 8) | input[packet_pos + 1];
		packet_pos += 2;

		unsigned short arcount = (input[packet_pos] << 8) | input[packet_pos + 1];
		packet_pos += 2;

		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "qdcount: " + ConvToStr(qdcount) + " ancount: " + ConvToStr(ancount) + " nscount: " + ConvToStr(nscount) + " arcount: " + ConvToStr(arcount));

		if (qdcount != 1)
			throw Exception("Question count != 1 in incoming packet");

		this->question = this->UnpackQuestion(input, len, packet_pos);

		for (unsigned i = 0; i < ancount; ++i)
			this->answers.push_back(this->UnpackResourceRecord(input, len, packet_pos));
	}

	unsigned short Pack(unsigned char* output, unsigned short output_size)
	{
		if (output_size < HEADER_LENGTH)
			throw Exception("Unable to pack oversized packet header");

		unsigned short pos = 0;

		output[pos++] = this->id >> 8;
		output[pos++] = this->id & 0xFF;
		output[pos++] = this->flags >> 8;
		output[pos++] = this->flags & 0xFF;
		output[pos++] = 0; // Question count, high byte
		output[pos++] = 1; // Question count, low byte
		output[pos++] = 0; // Answer count, high byte
		output[pos++] = 0; // Answer count, low byte
		output[pos++] = 0;
		output[pos++] = 0;
		output[pos++] = 0;
		output[pos++] = 0;

		{
			Question& q = this->question;

			if (q.type == QUERY_PTR)
			{
				irc::sockets::sockaddrs ip;
				if (!irc::sockets::aptosa(q.name, 0, ip))
					throw Exception("Unable to pack packet with malformed IP for PTR lookup");

				if (q.name.find(':') != std::string::npos)
				{
					// ::1 => 1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.ip6.arpa
					static const char* const hex = "0123456789abcdef";
					char reverse_ip[128];
					unsigned reverse_ip_count = 0;
					for (int j = 15; j >= 0; --j)
					{
						reverse_ip[reverse_ip_count++] = hex[ip.in6.sin6_addr.s6_addr[j] & 0xF];
						reverse_ip[reverse_ip_count++] = '.';
						reverse_ip[reverse_ip_count++] = hex[ip.in6.sin6_addr.s6_addr[j] >> 4];
						reverse_ip[reverse_ip_count++] = '.';
					}
					reverse_ip[reverse_ip_count++] = 0;

					q.name = reverse_ip;
					q.name += "ip6.arpa";
				}
				else
				{
					// 127.0.0.1 => 1.0.0.127.in-addr.arpa
					unsigned long forward = ip.in4.sin_addr.s_addr;
					ip.in4.sin_addr.s_addr = forward << 24 | (forward & 0xFF00) << 8 | (forward & 0xFF0000) >> 8 | forward >> 24;

					q.name = ip.addr() + ".in-addr.arpa";
				}
			}

			this->PackName(output, output_size, pos, q.name);

			if (pos + 4 >= output_size)
				throw Exception("Unable to pack oversized packet body");

			short s = htons(q.type);
			memcpy(&output[pos], &s, 2);
			pos += 2;

			// Query class, always IN
			output[pos++] = 0;
			output[pos++] = 1;
		}

		return pos;
	}
};

class MyManager : public Manager, public Timer, public EventHandler
{
	typedef TR1NS::unordered_map<Question, Query, Question::hash> cache_map;
	cache_map cache;

	irc::sockets::sockaddrs myserver;
	bool unloading;

	/** Maximum number of entries in cache
	 */
	static const unsigned int MAX_CACHE_SIZE = 1000;

	static bool IsExpired(const Query& record, time_t now = ServerInstance->Time())
	{
		const ResourceRecord& req = record.answers[0];
		return (req.created + static_cast<time_t>(req.ttl) < now);
	}

	/** Check the DNS cache to see if request can be handled by a cached result
	 * @return true if a cached result was found.
	 */
	bool CheckCache(DNS::Request* req, const DNS::Question& question)
	{
		ServerInstance->Logs->Log(MODNAME, LOG_SPARSE, "cache: Checking cache for " + question.name);

		cache_map::iterator it = this->cache.find(question);
		if (it == this->cache.end())
			return false;

		Query& record = it->second;
		if (IsExpired(record))
		{
			this->cache.erase(it);
			return false;
		}

		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "cache: Using cached result for " + question.name);
		record.cached = true;
		req->OnLookupComplete(&record);
		return true;
	}

	/** Add a record to the dns cache
	 * @param r The record
	 */
	void AddCache(Query& r)
	{
		if (cache.size() >= MAX_CACHE_SIZE)
			cache.clear();

		// Determine the lowest TTL value and use that as the TTL of the cache entry
		unsigned int cachettl = UINT_MAX;
		for (std::vector<ResourceRecord>::const_iterator i = r.answers.begin(); i != r.answers.end(); ++i)
		{
			const ResourceRecord& rr = *i;
			if (rr.ttl < cachettl)
				cachettl = rr.ttl;
		}

		cachettl = std::min(cachettl, (unsigned int)5*60);
		ResourceRecord& rr = r.answers.front();
		// Set TTL to what we've determined to be the lowest
		rr.ttl = cachettl;
		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "cache: added cache for " + rr.name + " -> " + rr.rdata + " ttl: " + ConvToStr(rr.ttl));
		this->cache[r.question] = r;
	}

 public:
	DNS::Request* requests[MAX_REQUEST_ID+1];

	MyManager(Module* c) : Manager(c), Timer(5*60, true)
		, unloading(false)
	{
		for (unsigned int i = 0; i <= MAX_REQUEST_ID; ++i)
			requests[i] = NULL;
		ServerInstance->Timers.AddTimer(this);
	}

	~MyManager()
	{
		// Ensure Process() will fail for new requests
		Close();
		unloading = true;

		for (unsigned int i = 0; i <= MAX_REQUEST_ID; ++i)
		{
			DNS::Request* request = requests[i];
			if (!request)
				continue;

			Query rr(request->question);
			rr.error = ERROR_UNKNOWN;
			request->OnError(&rr);

			delete request;
		}
	}

	void Close()
	{
		// Shutdown the socket if it exists.
		if (HasFd())
		{
			SocketEngine::Shutdown(this, 2);
			SocketEngine::Close(this);
		}

		// Remove all entries from the cache.
		cache.clear();
	}

	void Process(DNS::Request* req) CXX11_OVERRIDE
	{
		if ((unloading) || (req->creator->dying))
			throw Exception("Module is being unloaded");

		if (!HasFd())
		{
			Query rr(req->question);
			rr.error = ERROR_DISABLED;
			req->OnError(&rr);
			return;
		}

		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Processing request to lookup " + req->question.name + " of type " + ConvToStr(req->question.type) + " to " + this->myserver.addr());

		/* Create an id */
		unsigned int tries = 0;
		int id;
		do
		{
			id = ServerInstance->GenRandomInt(DNS::MAX_REQUEST_ID+1);

			if (++tries == DNS::MAX_REQUEST_ID*5)
			{
				// If we couldn't find an empty slot this many times, do a sequential scan as a last
				// resort. If an empty slot is found that way, go on, otherwise throw an exception
				id = -1;
				for (unsigned int i = 0; i <= DNS::MAX_REQUEST_ID; i++)
				{
					if (!this->requests[i])
					{
						id = i;
						break;
					}
				}

				if (id == -1)
					throw Exception("DNS: All ids are in use");

				break;
			}
		}
		while (this->requests[id]);

		req->id = id;
		this->requests[req->id] = req;

		Packet p;
		p.flags = QUERYFLAGS_RD;
		p.id = req->id;
		p.question = req->question;

		unsigned char buffer[524];
		unsigned short len = p.Pack(buffer, sizeof(buffer));

		/* Note that calling Pack() above can actually change the contents of p.question.name, if the query is a PTR,
		 * to contain the value that would be in the DNS cache, which is why this is here.
		 */
		if (req->use_cache && this->CheckCache(req, p.question))
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Using cached result");
			delete req;
			return;
		}

		// For PTR lookups we rewrite the original name to use the special in-addr.arpa/ip6.arpa
		// domains so we need to update the original request so that question checking works.
		req->question.name = p.question.name;

		if (SocketEngine::SendTo(this, buffer, len, 0, this->myserver) != len)
			throw Exception("DNS: Unable to send query");

		// Add timer for timeout
		ServerInstance->Timers.AddTimer(req);
	}

	void RemoveRequest(DNS::Request* req) CXX11_OVERRIDE
	{
		if (requests[req->id] == req)
			requests[req->id] = NULL;
	}

	std::string GetErrorStr(Error e) CXX11_OVERRIDE
	{
		switch (e)
		{
			case ERROR_UNLOADED:
				return "Module is unloading";
			case ERROR_TIMEDOUT:
				return "Request timed out";
			case ERROR_NOT_AN_ANSWER:
			case ERROR_NONSTANDARD_QUERY:
			case ERROR_FORMAT_ERROR:
			case ERROR_MALFORMED:
				return "Malformed answer";
			case ERROR_SERVER_FAILURE:
			case ERROR_NOT_IMPLEMENTED:
			case ERROR_REFUSED:
			case ERROR_INVALIDTYPE:
				return "Nameserver failure";
			case ERROR_DOMAIN_NOT_FOUND:
			case ERROR_NO_RECORDS:
				return "Domain not found";
			case ERROR_DISABLED:
				return "DNS lookups are disabled";
			case ERROR_NONE:
			case ERROR_UNKNOWN:
			default:
				return "Unknown error";
		}
	}

	std::string GetTypeStr(QueryType qt) CXX11_OVERRIDE
	{
		switch (qt)
		{
			case QUERY_A:
				return "A";
			case QUERY_AAAA:
				return "AAAA";
			case QUERY_CNAME:
				return "CNAME";
			case QUERY_PTR:
				return "PTR";
			case QUERY_TXT:
				return "TXT";
			default:
				return "UNKNOWN";
		}
	}

	void OnEventHandlerError(int errcode) CXX11_OVERRIDE
	{
		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "UDP socket got an error event");
	}

	void OnEventHandlerRead() CXX11_OVERRIDE
	{
		unsigned char buffer[524];
		irc::sockets::sockaddrs from;
		socklen_t x = sizeof(from);

		int length = SocketEngine::RecvFrom(this, buffer, sizeof(buffer), 0, &from.sa, &x);

		if (length < Packet::HEADER_LENGTH)
			return;

		if (myserver != from)
		{
			std::string server1 = from.str();
			std::string server2 = myserver.str();
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Got a result from the wrong server! Bad NAT or DNS forging attempt? '%s' != '%s'",
				server1.c_str(), server2.c_str());
			return;
		}

		Packet recv_packet;
		bool valid = false;

		try
		{
			recv_packet.Fill(buffer, length);
			valid = true;
		}
		catch (Exception& ex)
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, ex.GetReason());
		}

		// recv_packet.id must be filled in here
		DNS::Request* request = this->requests[recv_packet.id];
		if (request == NULL)
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Received an answer for something we didn't request");
			return;
		}

		if (request->question != recv_packet.question)
		{
			// This can happen under high latency, drop it silently, do not fail the request
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Received an answer that isn't for a question we asked");
			return;
		}

		if (!valid)
		{
			ServerInstance->stats.DnsBad++;
			recv_packet.error = ERROR_MALFORMED;
			request->OnError(&recv_packet);
		}
		else if (recv_packet.flags & QUERYFLAGS_OPCODE)
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Received a nonstandard query");
			ServerInstance->stats.DnsBad++;
			recv_packet.error = ERROR_NONSTANDARD_QUERY;
			request->OnError(&recv_packet);
		}
		else if (!(recv_packet.flags & QUERYFLAGS_QR) || (recv_packet.flags & QUERYFLAGS_RCODE))
		{
			Error error = ERROR_UNKNOWN;

			switch (recv_packet.flags & QUERYFLAGS_RCODE)
			{
				case 1:
					ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "format error");
					error = ERROR_FORMAT_ERROR;
					break;
				case 2:
					ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "server error");
					error = ERROR_SERVER_FAILURE;
					break;
				case 3:
					ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "domain not found");
					error = ERROR_DOMAIN_NOT_FOUND;
					break;
				case 4:
					ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "not implemented");
					error = ERROR_NOT_IMPLEMENTED;
					break;
				case 5:
					ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "refused");
					error = ERROR_REFUSED;
					break;
				default:
					break;
			}

			ServerInstance->stats.DnsBad++;
			recv_packet.error = error;
			request->OnError(&recv_packet);
		}
		else if (recv_packet.answers.empty())
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "No resource records returned");
			ServerInstance->stats.DnsBad++;
			recv_packet.error = ERROR_NO_RECORDS;
			request->OnError(&recv_packet);
		}
		else
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Lookup complete for " + request->question.name);
			ServerInstance->stats.DnsGood++;
			request->OnLookupComplete(&recv_packet);
			this->AddCache(recv_packet);
		}

		ServerInstance->stats.Dns++;

		/* Request's destructor removes it from the request map */
		delete request;
	}

	bool Tick(time_t now) CXX11_OVERRIDE
	{
		unsigned long expired = 0;
		for (cache_map::iterator it = this->cache.begin(); it != this->cache.end(); )
		{
			const Query& query = it->second;
			if (IsExpired(query, now))
			{
				expired++;
				this->cache.erase(it++);
			}
			else
				++it;
		}

		if (expired)
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "cache: purged %lu expired DNS entries", expired);

		return true;
	}

	void Rehash(const std::string& dnsserver, std::string sourceaddr, unsigned int sourceport)
	{
		irc::sockets::aptosa(dnsserver, DNS::PORT, myserver);

		/* Initialize mastersocket */
		Close();
		int s = socket(myserver.family(), SOCK_DGRAM, 0);
		this->SetFd(s);

		/* Have we got a socket? */
		if (this->HasFd())
		{
			SocketEngine::SetReuse(s);
			SocketEngine::NonBlocking(s);

			irc::sockets::sockaddrs bindto;
			if (sourceaddr.empty())
			{
				// set a sourceaddr for irc::sockets::aptosa() based on the servers af type
				if (myserver.family() == AF_INET)
					sourceaddr = "0.0.0.0";
				else if (myserver.family() == AF_INET6)
					sourceaddr = "::";
			}
			irc::sockets::aptosa(sourceaddr, sourceport, bindto);

			if (SocketEngine::Bind(this->GetFd(), bindto) < 0)
			{
				/* Failed to bind */
				ServerInstance->Logs->Log(MODNAME, LOG_SPARSE, "Error binding dns socket - hostnames will NOT resolve");
				SocketEngine::Close(this->GetFd());
				this->SetFd(-1);
			}
			else if (!SocketEngine::AddFd(this, FD_WANT_POLL_READ | FD_WANT_NO_WRITE))
			{
				ServerInstance->Logs->Log(MODNAME, LOG_SPARSE, "Internal error starting DNS - hostnames will NOT resolve.");
				SocketEngine::Close(this->GetFd());
				this->SetFd(-1);
			}

			if (bindto.family() != myserver.family())
				ServerInstance->Logs->Log(MODNAME, LOG_SPARSE, "Nameserver address family differs from source address family - hostnames might not resolve");
		}
		else
		{
			ServerInstance->Logs->Log(MODNAME, LOG_SPARSE, "Error creating DNS socket - hostnames will NOT resolve");
		}
	}
};

class ModuleDNS : public Module
{
	MyManager manager;
	std::string DNSServer;
	std::string SourceIP;
	unsigned int SourcePort;

	void FindDNSServer()
	{
#ifdef _WIN32
		// attempt to look up their nameserver from the system
		ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "WARNING: <dns:server> not defined, attempting to find a working server in the system settings...");

		PFIXED_INFO pFixedInfo;
		DWORD dwBufferSize = sizeof(FIXED_INFO);
		pFixedInfo = (PFIXED_INFO) HeapAlloc(GetProcessHeap(), 0, sizeof(FIXED_INFO));

		if (pFixedInfo)
		{
			if (GetNetworkParams(pFixedInfo, &dwBufferSize) == ERROR_BUFFER_OVERFLOW)
			{
				HeapFree(GetProcessHeap(), 0, pFixedInfo);
				pFixedInfo = (PFIXED_INFO) HeapAlloc(GetProcessHeap(), 0, dwBufferSize);
			}

			if (pFixedInfo)
			{
				if (GetNetworkParams(pFixedInfo, &dwBufferSize) == NO_ERROR)
					DNSServer = pFixedInfo->DnsServerList.IpAddress.String;

				HeapFree(GetProcessHeap(), 0, pFixedInfo);
			}

			if (!DNSServer.empty())
			{
				ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "<dns:server> set to '%s' as first active resolver in the system settings.", DNSServer.c_str());
				return;
			}
		}

		ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "No viable nameserver found! Defaulting to nameserver '127.0.0.1'!");
#else
		// attempt to look up their nameserver from /etc/resolv.conf
		ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "WARNING: <dns:server> not defined, attempting to find working server in /etc/resolv.conf...");

		std::ifstream resolv("/etc/resolv.conf");

		while (resolv >> DNSServer)
		{
			if (DNSServer == "nameserver")
			{
				resolv >> DNSServer;
				if (DNSServer.find_first_not_of("0123456789.") == std::string::npos || DNSServer.find_first_not_of("0123456789ABCDEFabcdef:") == std::string::npos)
				{
					ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "<dns:server> set to '%s' as first resolver in /etc/resolv.conf.",DNSServer.c_str());
					return;
				}
			}
		}

		ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "/etc/resolv.conf contains no viable nameserver entries! Defaulting to nameserver '127.0.0.1'!");
#endif
		DNSServer = "127.0.0.1";
	}

 public:
	ModuleDNS() : manager(this)
		, SourcePort(0)
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("dns");
		if (!tag->getBool("enabled", true))
		{
			// Clear these so they get reset if DNS is enabled again.
			DNSServer.clear();
			SourceIP.clear();
			SourcePort = 0;

			this->manager.Close();
			return;
		}

		const std::string oldserver = DNSServer;
		DNSServer = tag->getString("server");

		const std::string oldip = SourceIP;
		SourceIP = tag->getString("sourceip");

		const unsigned int oldport = SourcePort;
		SourcePort = tag->getUInt("sourceport", 0, 0, UINT16_MAX);

		if (DNSServer.empty())
			FindDNSServer();

		if (oldserver != DNSServer || oldip != SourceIP || oldport != SourcePort)
			this->manager.Rehash(DNSServer, SourceIP, SourcePort);
	}

	void OnUnloadModule(Module* mod) CXX11_OVERRIDE
	{
		for (unsigned int i = 0; i <= MAX_REQUEST_ID; ++i)
		{
			DNS::Request* req = this->manager.requests[i];
			if (!req)
				continue;

			if (req->creator == mod)
			{
				Query rr(req->question);
				rr.error = ERROR_UNLOADED;
				req->OnError(&rr);

				delete req;
			}
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support for DNS lookups", VF_CORE|VF_VENDOR);
	}
};

MODULE_INIT(ModuleDNS)

