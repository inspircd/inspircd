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

#pragma once

namespace DNS
{
	/** Valid query types
	 */
	enum QueryType
	{
		/* Nothing */
		QUERY_NONE,
		/* A simple A lookup */
		QUERY_A = 1,
		/* A CNAME lookup */
		QUERY_CNAME = 5,
		/* Reverse DNS lookup */
		QUERY_PTR = 12,
		/* IPv6 AAAA lookup */
		QUERY_AAAA = 28
	};

	/** Flags that can be AND'd into DNSPacket::flags to receive certain values
	 */
	enum
	{
		QUERYFLAGS_QR = 0x8000,
		QUERYFLAGS_OPCODE = 0x7800,
		QUERYFLAGS_AA = 0x400,
		QUERYFLAGS_TC = 0x200,
		QUERYFLAGS_RD = 0x100,
		QUERYFLAGS_RA = 0x80,
		QUERYFLAGS_Z = 0x70,
		QUERYFLAGS_RCODE = 0xF
	};

	enum Error
	{
		ERROR_NONE,
		ERROR_UNKNOWN,
		ERROR_UNLOADED,
		ERROR_TIMEDOUT,
		ERROR_NOT_AN_ANSWER,
		ERROR_NONSTANDARD_QUERY,
		ERROR_FORMAT_ERROR,
		ERROR_SERVER_FAILURE,
		ERROR_DOMAIN_NOT_FOUND,
		ERROR_NOT_IMPLEMENTED,
		ERROR_REFUSED,
		ERROR_NO_RECORDS,
		ERROR_INVALIDTYPE
	};

	const int PORT = 53;

	/**
	 * The maximum value of a dns request id,
	 * 16 bits wide, 0xFFFF.
	 */
	const int MAX_REQUEST_ID = 0xFFFF;

	class Exception : public ModuleException
	{
	 public:
		Exception(const std::string& message) : ModuleException(message) { }
	};

	struct Question
	{
		std::string name;
		QueryType type;
		unsigned short qclass;

		Question() : type(QUERY_NONE), qclass(0) { }
		Question(const std::string& n, QueryType t, unsigned short c = 1) : name(n), type(t), qclass(c) { }
		inline bool operator==(const Question& other) const { return name == other.name && type == other.type && qclass == other.qclass; }

		struct hash
		{
			size_t operator()(const Question& question) const
			{
				return irc::insensitive()(question.name);
			}
		};
	};

	struct ResourceRecord : Question
	{
		unsigned int ttl;
		std::string rdata;
		time_t created;

		ResourceRecord(const std::string& n, QueryType t, unsigned short c = 1) : Question(n, t, c), ttl(0), created(ServerInstance->Time()) { }
		ResourceRecord(const Question& question) : Question(question), ttl(0), created(ServerInstance->Time()) { }
	};

	struct Query
	{
		std::vector<Question> questions;
		std::vector<ResourceRecord> answers;
		Error error;
		bool cached;

		Query() : error(ERROR_NONE), cached(false) { }
		Query(const Question& question) : error(ERROR_NONE), cached(false) { questions.push_back(question); }
	};

	class ReplySocket;
	class Request;

	/** DNS manager
	 */
	class Manager : public DataProvider
	{
	 public:
		Manager(Module* mod) : DataProvider(mod, "DNS") { }

		virtual void Process(Request* req) = 0;
		virtual void RemoveRequest(Request* req) = 0;
		virtual std::string GetErrorStr(Error) = 0;
	};

	/** A DNS query.
	 */
	class Request : public Timer, public Question
	{
	 protected:
		Manager* const manager;
	 public:
		/* Use result cache if available */
		bool use_cache;
		/* Request id */
	 	unsigned short id;
	 	/* Creator of this request */
		Module* const creator;

		Request(Manager* mgr, Module* mod, const std::string& addr, QueryType qt, bool usecache = true)
			: Timer((ServerInstance->Config->dns_timeout ? ServerInstance->Config->dns_timeout : 5))
			, Question(addr, qt)
			, manager(mgr)
			, use_cache(usecache)
			, id(0)
			, creator(mod)
		{
			ServerInstance->Timers.AddTimer(this);
		}

		virtual ~Request()
		{
			manager->RemoveRequest(this);
		}

		/** Called when this request succeeds
		 * @param r The query sent back from the nameserver
		 */
		virtual void OnLookupComplete(const Query* req) = 0;

		/** Called when this request fails or times out.
		 * @param r The query sent back from the nameserver, check the error code.
		 */
		virtual void OnError(const Query* req) { }

		/** Used to time out the query, calls OnError and asks the TimerManager
		 * to delete this request
		 */
		bool Tick(time_t now)
		{
			Query rr(*this);
			rr.error = ERROR_TIMEDOUT;
			this->OnError(&rr);
			delete this;
			return false;
		}
	};

} // namespace DNS
