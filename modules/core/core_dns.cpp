/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2015, 2018-2023, 2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014-2016 Attila Molnar <attilamolnar@hush.com>
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
#include "modules/stats.h"
#include "stringutils.h"

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

/** A full packet sent or received to/from the nameserver
 */
class Packet final
	: public DNS::Query
{
private:
	const Module* creator;

	void PackName(unsigned char* output, unsigned short output_size, unsigned short& pos, const std::string& name)
	{
		if (pos + name.length() + 2 > output_size)
			throw DNS::Exception(creator, "Unable to pack name");

		ServerInstance->Logs.Debug(MODNAME, "Packing name {}", name);

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
		unsigned short pos_ptr = pos;
		unsigned short lowest_ptr = input_size;
		bool compressed = false;

		if (pos_ptr >= input_size)
			throw DNS::Exception(creator, "Unable to unpack name - no input");

		while (input[pos_ptr] > 0)
		{
			unsigned short offset = input[pos_ptr];

			if (offset & POINTER)
			{
				if ((offset & POINTER) != POINTER)
					throw DNS::Exception(creator, "Unable to unpack name - bogus compression header");
				if (pos_ptr + 1 >= input_size)
					throw DNS::Exception(creator, "Unable to unpack name - bogus compression header");

				/* Place pos at the second byte of the first (farthest) compression pointer */
				if (!compressed)
				{
					++pos;
					compressed = true;
				}

				pos_ptr = (offset & LABEL) << 8 | input[pos_ptr + 1];

				/* Pointers can only go back */
				if (pos_ptr >= lowest_ptr)
					throw DNS::Exception(creator, "Unable to unpack name - bogus compression pointer");
				lowest_ptr = pos_ptr;
			}
			else
			{
				if (pos_ptr + offset + 1 >= input_size)
					throw DNS::Exception(creator, "Unable to unpack name - offset too large");
				if (!name.empty())
					name += ".";
				for (unsigned i = 1; i <= offset; ++i)
					name += input[pos_ptr + i];

				pos_ptr += offset + 1;
				if (!compressed)
					/* Move up pos */
					pos = pos_ptr;
			}
		}

		/* +1 pos either to one byte after the compression pointer or one byte after the ending \0 */
		++pos;

		if (name.empty())
			throw DNS::Exception(creator, "Unable to unpack name - no name");

		ServerInstance->Logs.Debug(MODNAME, "Unpack name {}", name);

		return name;
	}

	DNS::Question UnpackQuestion(const unsigned char* input, unsigned short input_size, unsigned short& pos)
	{
		DNS::Question q;

		q.name = this->UnpackName(input, input_size, pos);

		if (pos + 4 > input_size)
			throw DNS::Exception(creator, "Unable to unpack question");

		q.type = static_cast<DNS::QueryType>(input[pos] << 8 | input[pos + 1]);
		pos += 2;

		// Skip over query class code
		pos += 2;

		return q;
	}

	DNS::ResourceRecord UnpackResourceRecord(const unsigned char* input, unsigned short input_size, unsigned short& pos)
	{
		auto record = static_cast<DNS::ResourceRecord>(this->UnpackQuestion(input, input_size, pos));

		if (pos + 6 > input_size)
			throw DNS::Exception(creator, "Unable to unpack resource record");

		record.ttl = (input[pos] << 24) | (input[pos + 1] << 16) | (input[pos + 2] << 8) | input[pos + 3];
		pos += 4;

		uint16_t rdlength = input[pos] << 8 | input[pos + 1];
		pos += 2;

		switch (record.type)
		{
			case DNS::QUERY_A:
			{
				if (pos + 4 > input_size)
					throw DNS::Exception(creator, "Unable to unpack A resource record");

				irc::sockets::sockaddrs addrs;
				addrs.in4.sin_family = AF_INET;
				addrs.in4.sin_addr.s_addr = input[pos] | (input[pos + 1] << 8) | (input[pos + 2] << 16)  | (input[pos + 3] << 24);
				pos += 4;

				record.rdata = addrs.addr();
				break;
			}
			case DNS::QUERY_AAAA:
			{
				if (pos + 16 > input_size)
					throw DNS::Exception(creator, "Unable to unpack AAAA resource record");

				irc::sockets::sockaddrs addrs;
				addrs.in6.sin6_family = AF_INET6;
				for (int j = 0; j < 16; ++j)
					addrs.in6.sin6_addr.s6_addr[j] = input[pos + j];
				pos += 16;

				record.rdata = addrs.addr();

				break;
			}
			case DNS::QUERY_CNAME:
			case DNS::QUERY_PTR:
			{
				record.rdata = this->UnpackName(input, input_size, pos);
				if (!InspIRCd::IsHost(record.rdata, true))
					throw DNS::Exception(creator, "Invalid name in CNAME/PTR resource record");

				break;
			}
			case DNS::QUERY_TXT:
			{
				if (pos + rdlength > input_size)
					throw DNS::Exception(creator, "Unable to unpack TXT resource record");

				record.rdata = std::string(reinterpret_cast<const char* >(input + pos), rdlength);
				pos += rdlength;

				if (record.rdata.find_first_of("\r\n\0", 0, 3) != std::string::npos)
					throw DNS::Exception(creator, "Invalid character in TXT resource record");

				break;
			}
			case DNS::QUERY_SRV:
			{
				if (rdlength < 6 || pos + rdlength > input_size)
					throw DNS::Exception(creator, "Unable to unpack SRV resource record");

				auto srv = std::make_shared<DNS::Record::SRV>();

				srv->priority = input[pos] << 8 | input[pos + 1];
				pos += 2;

				srv->weight = input[pos] << 8 | input[pos + 1];
				pos += 2;

				srv->port = input[pos] << 8 | input[pos + 1];
				pos += 2;

				srv->host = this->UnpackName(input, input_size, pos);
				if (!InspIRCd::IsHost(srv->host, true))
					throw DNS::Exception(creator, "Invalid name in SRV resource record");

				record.rdata = FMT::format("{} {} {} {}", srv->priority, srv->weight, srv->port, srv->host);
				record.rdataobj = srv;
				break;
			}
			default:
			{
				if (pos + rdlength > input_size)
					throw DNS::Exception(creator, "Unable to skip resource record");

				pos += rdlength;
				break;
			}
		}

		if (!record.name.empty() && !record.rdata.empty())
			ServerInstance->Logs.Debug(MODNAME, "{} -> {}", record.name, record.rdata);

		return record;
	}

public:
	static constexpr int POINTER = 0xC0;
	static constexpr int LABEL = 0x3F;
	static constexpr int HEADER_LENGTH = 12;

	/* ID for this packet */
	DNS::RequestId id = 0;

	/* Flags on the packet */
	unsigned short flags = 0;

	Packet(const Module* mod)
		: creator(mod)
	{
	}

	void Fill(const unsigned char* input, unsigned short len)
	{
		if (len < HEADER_LENGTH)
			throw DNS::Exception(creator, "Unable to fill packet");

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

		ServerInstance->Logs.Debug(MODNAME, "qdcount: {} ancount: {} nscount: {} arcount: {}",
			qdcount, ancount, nscount, arcount);

		if (qdcount != 1)
			throw DNS::Exception(creator, "Question count != 1 in incoming packet");

		this->question = this->UnpackQuestion(input, len, packet_pos);

		for (unsigned i = 0; i < ancount; ++i)
			this->answers.push_back(this->UnpackResourceRecord(input, len, packet_pos));
	}

	unsigned short Pack(unsigned char* output, unsigned short output_size)
	{
		if (output_size < HEADER_LENGTH)
			throw DNS::Exception(creator, "Unable to pack oversized packet header");

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
			auto& q = this->question;

			if (q.type == DNS::QUERY_PTR)
			{
				irc::sockets::sockaddrs ip(false);
				if (!ip.from_ip(q.name))
					throw DNS::Exception(creator, "Unable to pack packet with malformed IP for PTR lookup");

				if (q.name.find(':') != std::string::npos)
				{
					// ::1 => 1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.ip6.arpa
					char reverse_ip[128];
					unsigned reverse_ip_count = 0;
					for (int j = 15; j >= 0; --j)
					{
						reverse_ip[reverse_ip_count++] = Hex::TABLE_LOWER[ip.in6.sin6_addr.s6_addr[j] & 0xF];
						reverse_ip[reverse_ip_count++] = '.';
						reverse_ip[reverse_ip_count++] = Hex::TABLE_LOWER[ip.in6.sin6_addr.s6_addr[j] >> 4];
						reverse_ip[reverse_ip_count++] = '.';
					}
					reverse_ip[reverse_ip_count++] = 0;

					q.name = reverse_ip;
					q.name += "ip6.arpa";
				}
				else
				{
					// 127.0.0.1 => 1.0.0.127.in-addr.arpa
					unsigned int forward = ip.in4.sin_addr.s_addr;
					ip.in4.sin_addr.s_addr = forward << 24 | (forward & 0xFF00) << 8 | (forward & 0xFF0000) >> 8 | forward >> 24;

					q.name = ip.addr() + ".in-addr.arpa";
				}
			}

			this->PackName(output, output_size, pos, q.name);

			if (pos + 4 >= output_size)
				throw DNS::Exception(creator, "Unable to pack oversized packet body");

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

class MyManager final
	: public DNS::Manager
	, public Timer
	, public EventHandler
{
	typedef std::unordered_map<DNS::Question, DNS::Query, DNS::Question::hash> cache_map;
	cache_map cache;

	irc::sockets::sockaddrs myserver;
	bool unloading = false;

	/** Maximum number of entries in cache
	 */
	static constexpr unsigned int MAX_CACHE_SIZE = 1000;

	static bool IsExpired(const DNS::Query& record)
	{
		const auto& req = record.answers[0];
		return (req.created + static_cast<time_t>(req.ttl) < ServerInstance->Time());
	}

	/** Check the DNS cache to see if request can be handled by a cached result
	 * @return true if a cached result was found.
	 */
	bool CheckCache(DNS::Request* req, const DNS::Question& question)
	{
		ServerInstance->Logs.Critical(MODNAME, "cache: Checking cache for {}", question.name);

		cache_map::iterator it = this->cache.find(question);
		if (it == this->cache.end())
			return false;

		auto& record = it->second;
		if (IsExpired(record))
		{
			this->cache.erase(it);
			return false;
		}

		ServerInstance->Logs.Debug(MODNAME, "cache: Using cached result for {}", question.name);
		record.cached = true;
		req->OnLookupComplete(&record);
		return true;
	}

	/** Add a record to the dns cache
	 * @param r The record
	 */
	void AddCache(DNS::Query& r)
	{
		if (cache.size() >= MAX_CACHE_SIZE)
			cache.clear();

		// Determine the lowest TTL value and use that as the TTL of the cache entry
		unsigned int cachettl = UINT_MAX;
		for (const auto& rr : r.answers)
		{
			if (rr.ttl < cachettl)
				cachettl = rr.ttl;
		}

		cachettl = std::min<unsigned int>(cachettl, 5*60);
		auto& rr = r.answers.front();
		// Set TTL to what we've determined to be the lowest
		rr.ttl = cachettl;
		ServerInstance->Logs.Debug(MODNAME, "cache: added cache for {} -> {} ttl: {}", rr.name, rr.rdata, rr.ttl);
		this->cache[r.question] = r;
	}

public:
	DNS::Request* requests[DNS::MAX_REQUEST_ID + 1];
	size_t stats_total = 0;
	size_t stats_success = 0;
	size_t stats_failure = 0;
	unsigned long timeout = 0;

	MyManager(Module* c)
		: Manager(c)
		, Timer(5*60, true)
	{
		for (unsigned int i = 0; i <= DNS::MAX_REQUEST_ID; ++i)
			requests[i] = nullptr;
		ServerInstance->Timers.AddTimer(this);
	}

	~MyManager() override
	{
		// Ensure Process() will fail for new requests
		Close();
		unloading = true;

		for (unsigned int i = 0; i <= DNS::MAX_REQUEST_ID; ++i)
		{
			DNS::Request* request = requests[i];
			if (!request)
				continue;

			DNS::Query rr(request->question);
			rr.error = DNS::ERROR_UNKNOWN;
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

	unsigned long GetDefaultTimeout() const override
	{
		return timeout;
	}

	void Process(DNS::Request* req) override
	{
		if ((unloading) || (req->creator->dying))
			throw DNS::Exception(creator, "Module is being unloaded");

		if (!HasFd())
		{
			DNS::Query rr(req->question);
			rr.error = DNS::ERROR_DISABLED;
			req->OnError(&rr);
			return;
		}

		ServerInstance->Logs.Debug(MODNAME, "Processing request to lookup {} of type {} to {}",
			req->question.name, (int)req->question.type, this->myserver.addr());

		/* Create an id */
		unsigned int tries = 0;
		long id;
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
					throw DNS::Exception(creator, "DNS: All ids are in use");

				break;
			}
		}
		while (this->requests[id]);

		req->id = id;
		this->requests[req->id] = req;

		Packet p(creator);
		p.flags = DNS::QUERYFLAGS_RD;
		p.id = req->id;
		p.question = req->question;

		unsigned char buffer[524];
		unsigned short len = p.Pack(buffer, sizeof(buffer));

		/* Note that calling Pack() above can actually change the contents of p.question.name, if the query is a PTR,
		 * to contain the value that would be in the DNS cache, which is why this is here.
		 */
		if (req->use_cache && this->CheckCache(req, p.question))
		{
			ServerInstance->Logs.Debug(MODNAME, "Using cached result");
			delete req;
			return;
		}

		// For PTR lookups we rewrite the original name to use the special in-addr.arpa/ip6.arpa
		// domains so we need to update the original request so that question checking works.
		req->question.name = p.question.name;

		if (SocketEngine::SendTo(this, buffer, len, 0, this->myserver) != len)
			throw DNS::Exception(creator, "DNS: Unable to send query");

		// Add timer for timeout
		ServerInstance->Timers.AddTimer(req);
	}

	void RemoveRequest(DNS::Request* req) override
	{
		if (requests[req->id] == req)
			requests[req->id] = nullptr;
	}

	std::string GetErrorStr(DNS::Error e) override
	{
		switch (e)
		{
			case DNS::ERROR_UNLOADED:
				return "Module is unloading";
			case DNS::ERROR_TIMEDOUT:
				return "Request timed out";
			case DNS::ERROR_NOT_AN_ANSWER:
			case DNS::ERROR_NONSTANDARD_QUERY:
			case DNS::ERROR_FORMAT_ERROR:
			case DNS::ERROR_MALFORMED:
				return "Malformed answer";
			case DNS::ERROR_SERVER_FAILURE:
			case DNS::ERROR_NOT_IMPLEMENTED:
			case DNS::ERROR_REFUSED:
			case DNS::ERROR_INVALIDTYPE:
				return "Nameserver failure";
			case DNS::ERROR_DOMAIN_NOT_FOUND:
			case DNS::ERROR_NO_RECORDS:
				return "Domain not found";
			case DNS::ERROR_DISABLED:
				return "DNS lookups are disabled";
			case DNS::ERROR_NONE:
			case DNS::ERROR_UNKNOWN:
				break;
		}
		return "Unknown error";
	}

	std::string GetTypeStr(DNS::QueryType qt) override
	{
		switch (qt)
		{
			case DNS::QUERY_A:
				return "A";
			case DNS::QUERY_AAAA:
				return "AAAA";
			case DNS::QUERY_CNAME:
				return "CNAME";
			case DNS::QUERY_PTR:
				return "PTR";
			case DNS::QUERY_TXT:
				return "TXT";
			case DNS::QUERY_SRV:
				return "SRV";
			default:
				return "UNKNOWN";
		}
	}

	void OnEventHandlerError(int errcode) override
	{
		ServerInstance->Logs.Debug(MODNAME, "UDP socket got an error event");
	}

	void OnEventHandlerRead() override
	{
		unsigned char buffer[524];
		irc::sockets::sockaddrs from(false);
		socklen_t x = sizeof(from);

		ssize_t length = SocketEngine::RecvFrom(this, buffer, sizeof(buffer), 0, &from.sa, &x);

		if (length < Packet::HEADER_LENGTH)
			return;

		if (myserver != from)
		{
			std::string server1 = from.str();
			std::string server2 = myserver.str();
			ServerInstance->Logs.Debug(MODNAME, "Got a result from the wrong server! Bad NAT or DNS forging attempt? '{}' != '{}'",
				server1, server2);
			return;
		}

		Packet recv_packet(creator);
		bool valid = false;

		try
		{
			recv_packet.Fill(buffer, length);
			valid = true;
		}
		catch (const DNS::Exception& ex)
		{
			ServerInstance->Logs.Debug(MODNAME, ex.GetReason());
		}

		// recv_packet.id must be filled in here
		DNS::Request* request = this->requests[recv_packet.id];
		if (!request)
		{
			ServerInstance->Logs.Debug(MODNAME, "Received an answer for something we didn't request");
			return;
		}

		if (request->question != recv_packet.question)
		{
			// This can happen under high latency, drop it silently, do not fail the request
			ServerInstance->Logs.Debug(MODNAME, "Received an answer that isn't for a question we asked");
			return;
		}

		if (!valid)
		{
			this->stats_failure++;
			recv_packet.error = DNS::ERROR_MALFORMED;
			request->OnError(&recv_packet);
		}
		else if (recv_packet.flags & DNS::QUERYFLAGS_OPCODE)
		{
			ServerInstance->Logs.Debug(MODNAME, "Received a nonstandard query");
			this->stats_failure++;
			recv_packet.error = DNS::ERROR_NONSTANDARD_QUERY;
			request->OnError(&recv_packet);
		}
		else if (!(recv_packet.flags & DNS::QUERYFLAGS_QR) || (recv_packet.flags & DNS::QUERYFLAGS_RCODE))
		{
			auto error = DNS::ERROR_UNKNOWN;

			switch (recv_packet.flags & DNS::QUERYFLAGS_RCODE)
			{
				case 1:
					ServerInstance->Logs.Debug(MODNAME, "format error");
					error = DNS::ERROR_FORMAT_ERROR;
					break;
				case 2:
					ServerInstance->Logs.Debug(MODNAME, "server error");
					error = DNS::ERROR_SERVER_FAILURE;
					break;
				case 3:
					ServerInstance->Logs.Debug(MODNAME, "domain not found");
					error = DNS::ERROR_DOMAIN_NOT_FOUND;
					break;
				case 4:
					ServerInstance->Logs.Debug(MODNAME, "not implemented");
					error = DNS::ERROR_NOT_IMPLEMENTED;
					break;
				case 5:
					ServerInstance->Logs.Debug(MODNAME, "refused");
					error = DNS::ERROR_REFUSED;
					break;
				default:
					break;
			}

			this->stats_failure++;
			recv_packet.error = error;
			request->OnError(&recv_packet);
		}
		else if (recv_packet.answers.empty())
		{
			ServerInstance->Logs.Debug(MODNAME, "No resource records returned");
			this->stats_failure++;
			recv_packet.error = DNS::ERROR_NO_RECORDS;
			request->OnError(&recv_packet);
		}
		else
		{
			ServerInstance->Logs.Debug(MODNAME, "Lookup complete for {}", request->question.name);
			this->stats_success++;
			request->OnLookupComplete(&recv_packet);
			this->AddCache(recv_packet);
		}

		this->stats_total++;

		/* Request's destructor removes it from the request map */
		delete request;
	}

	bool Tick() override
	{
		unsigned long expired = 0;
		for (cache_map::iterator it = this->cache.begin(); it != this->cache.end(); )
		{
			const auto& query = it->second;
			if (IsExpired(query))
			{
				expired++;
				this->cache.erase(it++);
			}
			else
				++it;
		}

		if (expired)
			ServerInstance->Logs.Debug(MODNAME, "cache: purged {} expired DNS entries", expired);

		return true;
	}

	void Rehash(const std::string& dnsserver, std::string sourceaddr, in_port_t sourceport)
	{
		myserver.from_ip_port(dnsserver, DNS::PORT);

		/* Initialize mastersocket */
		Close();
		int s = socket(myserver.family(), SOCK_DGRAM, 0);
		this->SetFd(s);

		/* Have we got a socket? */
		if (this->HasFd())
		{
			SocketEngine::SetOption<int>(s, SOL_SOCKET, SO_REUSEADDR, 1);
			SocketEngine::NonBlocking(s);

			if (sourceaddr.empty())
			{
				// set a sourceaddr based on the servers af type
				if (myserver.family() == AF_INET)
					sourceaddr = "0.0.0.0";
				else if (myserver.family() == AF_INET6)
					sourceaddr = "::";
			}

			irc::sockets::sockaddrs bindto;
			bindto.from_ip_port(sourceaddr, sourceport);

			if (SocketEngine::Bind(this, bindto) < 0)
			{
				/* Failed to bind */
				ServerInstance->Logs.Critical(MODNAME, "Error binding dns socket - hostnames will NOT resolve");
				SocketEngine::Close(this->GetFd());
				this->SetFd(-1);
			}
			else if (!SocketEngine::AddFd(this, FD_WANT_POLL_READ | FD_WANT_NO_WRITE))
			{
				ServerInstance->Logs.Critical(MODNAME, "Internal error starting DNS - hostnames will NOT resolve.");
				SocketEngine::Close(this->GetFd());
				this->SetFd(-1);
			}

			if (bindto.family() != myserver.family())
				ServerInstance->Logs.Warning(MODNAME, "Nameserver address family differs from source address family - hostnames might not resolve");
		}
		else
		{
			ServerInstance->Logs.Critical(MODNAME, "Error creating DNS socket - hostnames will NOT resolve");
		}
	}
};

class ModuleDNS final
	: public Module
	, public Stats::EventListener
{
	MyManager manager;
	std::string DNSServer;
	std::string SourceIP;
	in_port_t SourcePort = 0;

	void FindDNSServer()
	{
#ifdef _WIN32
		// attempt to look up their nameserver from the system
		ServerInstance->Logs.Normal(MODNAME, "WARNING: <dns:server> not defined, attempting to find a working server in the system settings...");

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
				ServerInstance->Logs.Normal(MODNAME, "<dns:server> set to '{}' as first active resolver in the system settings.", DNSServer);
				return;
			}
		}

		ServerInstance->Logs.Warning(MODNAME, "No viable nameserver found! Defaulting to nameserver '127.0.0.1'!");
#else
		// attempt to look up their nameserver from /etc/resolv.conf
		ServerInstance->Logs.Normal(MODNAME, "WARNING: <dns:server> not defined, attempting to find working server in /etc/resolv.conf...");

		std::ifstream resolv("/etc/resolv.conf");

		while (resolv >> DNSServer)
		{
			if (DNSServer == "nameserver")
			{
				resolv >> DNSServer;
				if (DNSServer.find_first_not_of("0123456789.") == std::string::npos || DNSServer.find_first_not_of("0123456789ABCDEFabcdef:") == std::string::npos)
				{
					ServerInstance->Logs.Normal(MODNAME, "<dns:server> set to '{}' as first resolver in /etc/resolv.conf.", DNSServer);
					return;
				}
			}
		}

		ServerInstance->Logs.Warning(MODNAME, "/etc/resolv.conf contains no viable nameserver entries! Defaulting to nameserver '127.0.0.1'!");
#endif
		DNSServer = "127.0.0.1";
	}

public:
	ModuleDNS()
		: Module(VF_CORE | VF_VENDOR, "Provides support for DNS lookups")
		, Stats::EventListener(this)
		, manager(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("dns");
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

		const in_port_t oldport = SourcePort;
		SourcePort = tag->getNum<in_port_t>("sourceport", 0);

		if (DNSServer.empty())
			FindDNSServer();

		if (oldserver != DNSServer || oldip != SourceIP || oldport != SourcePort || !SourcePort)
			this->manager.Rehash(DNSServer, SourceIP, SourcePort);

		this->manager.timeout = tag->getDuration("timeout", 5, 1);
	}

	ModResult OnStats(Stats::Context& stats) override
	{
		if (stats.GetSymbol() == 'T')
		{
			stats.AddGenericRow(FMT::format("DNS requests: {} ({} succeeded, {} failed)",
				manager.stats_total, manager.stats_success, manager.stats_failure));
		}
		return MOD_RES_PASSTHRU;
	}

	void OnUnloadModule(Module* mod) override
	{
		for (unsigned int i = 0; i <= DNS::MAX_REQUEST_ID; ++i)
		{
			DNS::Request* req = this->manager.requests[i];
			if (!req)
				continue;

			if (req->creator == mod)
			{
				DNS::Query rr(req->question);
				rr.error = DNS::ERROR_UNLOADED;
				req->OnError(&rr);

				delete req;
			}
		}
	}
};

MODULE_INIT(ModuleDNS)

