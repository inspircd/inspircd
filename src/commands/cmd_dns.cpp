/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2003-2013 Anope Team <team@anope.org>
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

using namespace DNS;

/** A full packet sent or recieved to/from the nameserver
 */
class Packet : public Query
{
	void PackName(unsigned char* output, unsigned short output_size, unsigned short& pos, const std::string& name)
	{
		if (pos + name.length() + 2 > output_size)
			throw Exception("Unable to pack name");

		ServerInstance->Logs->Log("RESOLVER", LOG_DEBUG, "Resolver: Packing name " + name);

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

		ServerInstance->Logs->Log("RESOLVER", LOG_DEBUG, "Resolver: Unpack name " + name);

		return name;
	}

	Question UnpackQuestion(const unsigned char* input, unsigned short input_size, unsigned short& pos)
	{
		Question question;

		question.name = this->UnpackName(input, input_size, pos);

		if (pos + 4 > input_size)
			throw Exception("Unable to unpack question");

		question.type = static_cast<QueryType>(input[pos] << 8 | input[pos + 1]);
		pos += 2;

		question.qclass = input[pos] << 8 | input[pos + 1];
		pos += 2;

		return question;
	}

	ResourceRecord UnpackResourceRecord(const unsigned char* input, unsigned short input_size, unsigned short& pos)
	{
		ResourceRecord record = static_cast<ResourceRecord>(this->UnpackQuestion(input, input_size, pos));

		if (pos + 6 > input_size)
			throw Exception("Unable to unpack resource record");

		record.ttl = (input[pos] << 24) | (input[pos + 1] << 16) | (input[pos + 2] << 8) | input[pos + 3];
		pos += 4;

		//record.rdlength = input[pos] << 8 | input[pos + 1];
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
				break;
			}
			default:
				break;
		}

		if (!record.name.empty() && !record.rdata.empty())
			ServerInstance->Logs->Log("RESOLVER", LOG_DEBUG, "Resolver: " + record.name + " -> " + record.rdata);

		return record;
	}

 public:
	static const int POINTER = 0xC0;
	static const int LABEL = 0x3F;
	static const int HEADER_LENGTH = 12;

	/* ID for this packet */
	unsigned short id;
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

		if (this->id >= MAX_REQUEST_ID)
			throw Exception("Query ID too large?");

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

		ServerInstance->Logs->Log("RESOLVER", LOG_DEBUG, "Resolver: qdcount: " + ConvToStr(qdcount) + " ancount: " + ConvToStr(ancount) + " nscount: " + ConvToStr(nscount) + " arcount: " + ConvToStr(arcount));

		for (unsigned i = 0; i < qdcount; ++i)
			this->questions.push_back(this->UnpackQuestion(input, len, packet_pos));

		for (unsigned i = 0; i < ancount; ++i)
			this->answers.push_back(this->UnpackResourceRecord(input, len, packet_pos));
	}

	unsigned short Pack(unsigned char* output, unsigned short output_size)
	{
		if (output_size < HEADER_LENGTH)
			throw Exception("Unable to pack packet");

		unsigned short pos = 0;

		output[pos++] = this->id >> 8;
		output[pos++] = this->id & 0xFF;
		output[pos++] = this->flags >> 8;
		output[pos++] = this->flags & 0xFF;
		output[pos++] = this->questions.size() >> 8;
		output[pos++] = this->questions.size() & 0xFF;
		output[pos++] = this->answers.size() >> 8;
		output[pos++] = this->answers.size() & 0xFF;
		output[pos++] = 0;
		output[pos++] = 0;
		output[pos++] = 0;
		output[pos++] = 0;

		for (unsigned i = 0; i < this->questions.size(); ++i)
		{
			Question& q = this->questions[i];

			if (q.type == QUERY_PTR)
			{
				irc::sockets::sockaddrs ip;
				irc::sockets::aptosa(q.name, 0, ip);

				if (q.name.find(':') != std::string::npos)
				{
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
					unsigned long forward = ip.in4.sin_addr.s_addr;
					ip.in4.sin_addr.s_addr = forward << 24 | (forward & 0xFF00) << 8 | (forward & 0xFF0000) >> 8 | forward >> 24;

					q.name = ip.addr() + ".in-addr.arpa";
				}
			}

			this->PackName(output, output_size, pos, q.name);

			if (pos + 4 >= output_size)
				throw Exception("Unable to pack packet");

			short s = htons(q.type);
			memcpy(&output[pos], &s, 2);
			pos += 2;

			s = htons(q.qclass);
			memcpy(&output[pos], &s, 2);
			pos += 2;
		}

		for (unsigned int i = 0; i < answers.size(); i++)
		{
			ResourceRecord& rr = answers[i];

			this->PackName(output, output_size, pos, rr.name);

			if (pos + 8 >= output_size)
				throw Exception("Unable to pack packet");

			short s = htons(rr.type);
			memcpy(&output[pos], &s, 2);
			pos += 2;

			s = htons(rr.qclass);
			memcpy(&output[pos], &s, 2);
			pos += 2;

			long l = htonl(rr.ttl);
			memcpy(&output[pos], &l, 4);
			pos += 4;

			switch (rr.type)
			{
				case QUERY_A:
				{
					if (pos + 6 > output_size)
						throw Exception("Unable to pack packet");

					irc::sockets::sockaddrs a;
					irc::sockets::aptosa(rr.rdata, 0, a);

					s = htons(4);
					memcpy(&output[pos], &s, 2);
					pos += 2;

					memcpy(&output[pos], &a.in4.sin_addr, 4);
					pos += 4;
					break;
				}
				case QUERY_AAAA:
				{
					if (pos + 18 > output_size)
						throw Exception("Unable to pack packet");

					irc::sockets::sockaddrs a;
					irc::sockets::aptosa(rr.rdata, 0, a);

					s = htons(16);
					memcpy(&output[pos], &s, 2);
					pos += 2;

					memcpy(&output[pos], &a.in6.sin6_addr, 16);
					pos += 16;
					break;
				}
				case QUERY_CNAME:
				case QUERY_PTR:
				{
					if (pos + 2 >= output_size)
						throw Exception("Unable to pack packet");

					unsigned short packet_pos_save = pos;
					pos += 2;

					this->PackName(output, output_size, pos, rr.rdata);

					s = htons(pos - packet_pos_save - 2);
					memcpy(&output[packet_pos_save], &s, 2);
					break;
				}
				default:
					break;
			}
		}

		return pos;
	}
};

class MyManager : public Manager, public Timer, public EventHandler
{
	typedef TR1NS::unordered_map<Question, Query, Question::hash> cache_map;
	cache_map cache;

	irc::sockets::sockaddrs myserver;

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
		ServerInstance->Logs->Log("RESOLVER", LOG_SPARSE, "Resolver: cache: Checking cache for " + question.name);

		cache_map::iterator it = this->cache.find(question);
		if (it == this->cache.end())
			return false;

		Query& record = it->second;
		if (IsExpired(record))
		{
			this->cache.erase(it);
			return false;
		}

		ServerInstance->Logs->Log("RESOLVER", LOG_DEBUG, "Resolver: cache: Using cached result for " + question.name);
		record.cached = true;
		req->OnLookupComplete(&record);
		return true;
	}

	/** Add a record to the dns cache
	 * @param r The record
	 */
	void AddCache(Query& r)
	{
		const ResourceRecord& rr = r.answers[0];
		ServerInstance->Logs->Log("RESOLVER", LOG_DEBUG, "Resolver: cache: added cache for " + rr.name + " -> " + rr.rdata + " ttl: " + ConvToStr(rr.ttl));
		this->cache[r.questions[0]] = r;
	}

 public:
	DNS::Request* requests[MAX_REQUEST_ID];

	MyManager(Module* c) : Manager(c), Timer(3600, ServerInstance->Time(), true)
	{
		for (int i = 0; i < MAX_REQUEST_ID; ++i)
			requests[i] = NULL;
		ServerInstance->Timers->AddTimer(this);
	}

	~MyManager()
	{
		for (int i = 0; i < MAX_REQUEST_ID; ++i)
		{
			DNS::Request* request = requests[i];
			if (!request)
				continue;

			Query rr(*request);
			rr.error = ERROR_UNKNOWN;
			request->OnError(&rr);

			delete request;
		}
	}

	void Process(DNS::Request* req)
	{
		ServerInstance->Logs->Log("RESOLVER", LOG_DEBUG, "Resolver: Processing request to lookup " + req->name + " of type " + ConvToStr(req->type) + " to " + this->myserver.addr());

		/* Create an id */
		unsigned int tries = 0;
		do
		{
			req->id = ServerInstance->GenRandomInt(DNS::MAX_REQUEST_ID);

			if (++tries == DNS::MAX_REQUEST_ID*5)
			{
				// If we couldn't find an empty slot this many times, do a sequential scan as a last
				// resort. If an empty slot is found that way, go on, otherwise throw an exception
				req->id = 0;
				for (int i = 1; i < DNS::MAX_REQUEST_ID; i++)
				{
					if (!this->requests[i])
					{
						req->id = i;
						break;
					}
				}

				if (req->id == 0)
					throw Exception("DNS: All ids are in use");

				break;
			}
		}
		while (!req->id || this->requests[req->id]);

		this->requests[req->id] = req;

		Packet p;
		p.flags = QUERYFLAGS_RD;
		p.id = req->id;
		p.questions.push_back(*req);

		unsigned char buffer[524];
		unsigned short len = p.Pack(buffer, sizeof(buffer));

		/* Note that calling Pack() above can actually change the contents of p.questions[0].name, if the query is a PTR,
		 * to contain the value that would be in the DNS cache, which is why this is here.
		 */
		if (req->use_cache && this->CheckCache(req, p.questions[0]))
		{
			ServerInstance->Logs->Log("RESOLVER", LOG_DEBUG, "Resolver: Using cached result");
			delete req;
			return;
		}

		if (ServerInstance->SE->SendTo(this, buffer, len, 0, &this->myserver.sa, this->myserver.sa_size()) != len)
			throw Exception("DNS: Unable to send query");
	}

	void RemoveRequest(DNS::Request* req)
	{
		this->requests[req->id] = NULL;
	}

	std::string GetErrorStr(Error e)
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
				return "Malformed answer";
			case ERROR_SERVER_FAILURE:
			case ERROR_NOT_IMPLEMENTED:
			case ERROR_REFUSED:
			case ERROR_INVALIDTYPE:
				return "Nameserver failure";
			case ERROR_DOMAIN_NOT_FOUND:
			case ERROR_NO_RECORDS:
				return "Domain not found";
			case ERROR_NONE:
			case ERROR_UNKNOWN:
			default:
				return "Unknown error";
		}
	}

	void HandleEvent(EventType et, int)
	{
		if (et == EVENT_ERROR)
		{
			ServerInstance->Logs->Log("RESOLVER", LOG_DEBUG, "Resolver: UDP socket got an error event");
			return;
		}

		unsigned char buffer[524];
		irc::sockets::sockaddrs from;
		socklen_t x = sizeof(from);

		int length = ServerInstance->SE->RecvFrom(this, buffer, sizeof(buffer), 0, &from.sa, &x);

		if (length < Packet::HEADER_LENGTH)
			return;

		Packet recv_packet;

		try
		{
			recv_packet.Fill(buffer, length);
		}
		catch (Exception& ex)
		{
			ServerInstance->Logs->Log("RESOLVER", LOG_DEBUG, std::string(ex.GetReason()));
			return;
		}

		if (myserver != from)
		{
			std::string server1 = from.str();
			std::string server2 = myserver.str();
			ServerInstance->Logs->Log("RESOLVER", LOG_DEBUG, "Resolver: Got a result from the wrong server! Bad NAT or DNS forging attempt? '%s' != '%s'",
				server1.c_str(), server2.c_str());
			return;
		}

		DNS::Request* request = this->requests[recv_packet.id];
		if (request == NULL)
		{
			ServerInstance->Logs->Log("RESOLVER", LOG_DEBUG, "Resolver: Received an answer for something we didn't request");
			return;
		}

		if (recv_packet.flags & QUERYFLAGS_OPCODE)
		{
			ServerInstance->Logs->Log("RESOLVER", LOG_DEBUG, "Resolver: Received a nonstandard query");
			ServerInstance->stats->statsDnsBad++;
			recv_packet.error = ERROR_NONSTANDARD_QUERY;
			request->OnError(&recv_packet);
		}
		else if (recv_packet.flags & QUERYFLAGS_RCODE)
		{
			Error error = ERROR_UNKNOWN;

			switch (recv_packet.flags & QUERYFLAGS_RCODE)
			{
				case 1:
					ServerInstance->Logs->Log("RESOLVER", LOG_DEBUG, "Resolver: format error");
					error = ERROR_FORMAT_ERROR;
					break;
				case 2:
					ServerInstance->Logs->Log("RESOLVER", LOG_DEBUG, "Resolver: server error");
					error = ERROR_SERVER_FAILURE;
					break;
				case 3:
					ServerInstance->Logs->Log("RESOLVER", LOG_DEBUG, "Resolver: domain not found");
					error = ERROR_DOMAIN_NOT_FOUND;
					break;
				case 4:
					ServerInstance->Logs->Log("RESOLVER", LOG_DEBUG, "Resolver: not implemented");
					error = ERROR_NOT_IMPLEMENTED;
					break;
				case 5:
					ServerInstance->Logs->Log("RESOLVER", LOG_DEBUG, "Resolver: refused");
					error = ERROR_REFUSED;
					break;
				default:
					break;
			}

			ServerInstance->stats->statsDnsBad++;
			recv_packet.error = error;
			request->OnError(&recv_packet);
		}
		else if (recv_packet.questions.empty() || recv_packet.answers.empty())
		{
			ServerInstance->Logs->Log("RESOLVER", LOG_DEBUG, "Resolver: No resource records returned");
			ServerInstance->stats->statsDnsBad++;
			recv_packet.error = ERROR_NO_RECORDS;
			request->OnError(&recv_packet);
		}
		else
		{
			ServerInstance->Logs->Log("RESOLVER", LOG_DEBUG, "Resolver: Lookup complete for " + request->name);
			ServerInstance->stats->statsDnsGood++;
			request->OnLookupComplete(&recv_packet);
			this->AddCache(recv_packet);
		}

		ServerInstance->stats->statsDns++;

		/* Request's destructor removes it from the request map */
		delete request;
	}

	bool Tick(time_t now)
	{
		ServerInstance->Logs->Log("RESOLVER", LOG_DEBUG, "Resolver: cache: purging DNS cache");

		for (cache_map::iterator it = this->cache.begin(); it != this->cache.end(); )
		{
			const Query& query = it->second;
			if (IsExpired(query, now))
				this->cache.erase(it++);
			else
				++it;
		}
		return true;
	}

	void Rehash(const std::string& dnsserver)
	{
		if (this->GetFd() > -1)
		{
			ServerInstance->SE->DelFd(this);
			ServerInstance->SE->Shutdown(this, 2);
			ServerInstance->SE->Close(this);
			this->SetFd(-1);

			/* Remove expired entries from the cache */
			this->Tick(ServerInstance->Time());
		}

		irc::sockets::aptosa(dnsserver, DNS::PORT, myserver);

		/* Initialize mastersocket */
		int s = socket(myserver.sa.sa_family, SOCK_DGRAM, 0);
		this->SetFd(s);

		/* Have we got a socket? */
		if (this->GetFd() != -1)
		{
			ServerInstance->SE->SetReuse(s);
			ServerInstance->SE->NonBlocking(s);

			irc::sockets::sockaddrs bindto;
			memset(&bindto, 0, sizeof(bindto));
			bindto.sa.sa_family = myserver.sa.sa_family;

			if (ServerInstance->SE->Bind(this->GetFd(), bindto) < 0)
			{
				/* Failed to bind */
				ServerInstance->Logs->Log("RESOLVER", LOG_SPARSE, "Resolver: Error binding dns socket - hostnames will NOT resolve");
				ServerInstance->SE->Close(this);
				this->SetFd(-1);
			}
			else if (!ServerInstance->SE->AddFd(this, FD_WANT_POLL_READ | FD_WANT_NO_WRITE))
			{
				ServerInstance->Logs->Log("RESOLVER", LOG_SPARSE, "Resolver: Internal error starting DNS - hostnames will NOT resolve.");
				ServerInstance->SE->Close(this);
				this->SetFd(-1);
			}
		}
		else
		{
			ServerInstance->Logs->Log("RESOLVER", LOG_SPARSE, "Resolver: Error creating DNS socket - hostnames will NOT resolve");
		}
	}
};

class ModuleDNS : public Module
{
	MyManager manager;
	std::string DNSServer;

	void FindDNSServer()
	{
#ifdef _WIN32
		// attempt to look up their nameserver from the system
		ServerInstance->Logs->Log("CONFIG",LOG_DEFAULT,"WARNING: <dns:server> not defined, attempting to find a working server in the system settings...");

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
				ServerInstance->Logs->Log("CONFIG",LOG_DEFAULT,"<dns:server> set to '%s' as first active resolver in the system settings.", DNSServer.c_str());
				return;
			}
		}

		ServerInstance->Logs->Log("CONFIG",LOG_DEFAULT,"No viable nameserver found! Defaulting to nameserver '127.0.0.1'!");
#else
		// attempt to look up their nameserver from /etc/resolv.conf
		ServerInstance->Logs->Log("CONFIG",LOG_DEFAULT,"WARNING: <dns:server> not defined, attempting to find working server in /etc/resolv.conf...");

		std::ifstream resolv("/etc/resolv.conf");

		while (resolv >> DNSServer)
		{
			if (DNSServer == "nameserver")
			{
				resolv >> DNSServer;
				if (DNSServer.find_first_not_of("0123456789.") == std::string::npos)
				{
					ServerInstance->Logs->Log("CONFIG",LOG_DEFAULT,"<dns:server> set to '%s' as first resolver in /etc/resolv.conf.",DNSServer.c_str());
					return;
				}
			}
		}

		ServerInstance->Logs->Log("CONFIG",LOG_DEFAULT,"/etc/resolv.conf contains no viable nameserver entries! Defaulting to nameserver '127.0.0.1'!");
#endif
		DNSServer = "127.0.0.1";
	}

 public:
 	ModuleDNS() : manager(this)
	{
	}

	void init()
	{
		Implementation i[] = { I_OnRehash, I_OnUnloadModule };
		ServerInstance->Modules->Attach(i, this, sizeof(i) / sizeof(Implementation));

		ServerInstance->Modules->AddService(this->manager);

		this->OnRehash(NULL);
	}

	void OnRehash(User* user)
	{
		std::string oldserver = DNSServer;
		DNSServer = ServerInstance->Config->ConfValue("dns")->getString("server");
		if (DNSServer.empty())
			FindDNSServer();

		if (oldserver != DNSServer)
			this->manager.Rehash(DNSServer);
	}

	void OnUnloadModule(Module* mod)
	{
		for (int i = 0; i < MAX_REQUEST_ID; ++i)
		{
			DNS::Request* req = this->manager.requests[i];
			if (!req)
				continue;

			if (req->creator == mod)
			{
				Query rr(*req);
				rr.error = ERROR_UNLOADED;
				req->OnError(&rr);

				delete req;
			}
		}
	}

	Version GetVersion()
	{
		return Version("DNS support", VF_CORE|VF_VENDOR);
	}
};

MODULE_INIT(ModuleDNS)

