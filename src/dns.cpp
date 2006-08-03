/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *		       E-mail:
 *		<brain@chatspike.net>
 *		<Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/*
dns.cpp - Nonblocking DNS functions.
Very very loosely based on the firedns library,
Copyright (C) 2002 Ian Gulliver. This file is no
longer anything like firedns, there are many major
differences between this code and the original.
Please do not assume that firedns works like this,
looks like this, walks like this or tastes like this.
*/

using namespace std;

#include <string>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <map>
#include "dns.h"
#include "inspircd.h"
#include "helperfuncs.h"
#include "inspircd_config.h"
#include "socketengine.h"
#include "configreader.h"

/* We need these */
extern InspIRCd* ServerInstance;
extern ServerConfig* Config;

/* Master file descriptor */
int DNS::MasterSocket;

/* Masks to mask off the responses we get from the DNSRequest methods */
enum QueryInfo
{
	ERROR_MASK	= 0x10000	/* Result is an error */
};

/* Flags which can be ORed into a request or reply for different meanings */
enum QueryFlags
{
	FLAGS_MASK_RD		= 0x01,	/* Recursive */
	FLAGS_MASK_TC		= 0x02,
	FLAGS_MASK_AA		= 0x04,	/* Authoritative */
	FLAGS_MASK_OPCODE	= 0x78,
	FLAGS_MASK_QR		= 0x80,
	FLAGS_MASK_RCODE	= 0x0F,	/* Request */
	FLAGS_MASK_Z		= 0x70,
	FLAGS_MASK_RA 		= 0x80
};


/* Represents a dns resource record (rr) */
class ResourceRecord
{
 public:
	QueryType	type;		/* Record type */
	unsigned int	rr_class;	/* Record class */
	unsigned long	ttl;		/* Time to live */
	unsigned int	rdlength;	/* Record length */
};

/* Represents a dns request/reply header, and its payload as opaque data.
 */
class DNSHeader
{
 public:
	unsigned char	id[2];		/* Request id */
	unsigned int	flags1;		/* Flags */
	unsigned int	flags2;		/* Flags */
	unsigned int	qdcount;
	unsigned int	ancount;	/* Answer count */
	unsigned int	nscount;	/* Nameserver count */
	unsigned int	arcount;
	unsigned char	payload[512];	/* Packet payload */
};

/* Represents a request 'on the wire' with routing information relating to
 * where to call when we get a result
 */
class DNSRequest
{
 public:
	unsigned char   id[2];		/* Request id */
	unsigned char*	res;		/* Result processing buffer */
	unsigned int    rr_class;	/* Request class */
	QueryType       type;		/* Request type */
	insp_inaddr	myserver;	/* DNS server address*/

	/* Allocate the processing buffer */
	DNSRequest(insp_inaddr server)
	{
		res = new unsigned char[512];
		*res = 0;
		memcpy(&myserver, &server, sizeof(insp_inaddr));
	}

	/* Deallocate the processing buffer */
	~DNSRequest()
	{
		delete[] res;
	}

	/* Called when a result is ready to be processed which matches this id */
	DNSInfo ResultIsReady(DNSHeader &h, int length);

	/* Called when there are requests to be sent out */
	int SendRequests(const DNSHeader *header, const int length, QueryType qt);
};

/* Fill a ResourceRecord class based on raw data input */
inline void DNS::FillResourceRecord(ResourceRecord* rr, const unsigned char *input)
{
	rr->type = (QueryType)((input[0] << 8) + input[1]);
	rr->rr_class = (input[2] << 8) + input[3];
	rr->ttl = (input[4] << 24) + (input[5] << 16) + (input[6] << 8) + input[7];
	rr->rdlength = (input[8] << 8) + input[9];
}

/* Fill a DNSHeader class based on raw data input of a given length */
inline void DNS::FillHeader(DNSHeader *header, const unsigned char *input, const int length)
{
	header->id[0] = input[0];
	header->id[1] = input[1];
	header->flags1 = input[2];
	header->flags2 = input[3];
	header->qdcount = (input[4] << 8) + input[5];
	header->ancount = (input[6] << 8) + input[7];
	header->nscount = (input[8] << 8) + input[9];
	header->arcount = (input[10] << 8) + input[11];
	memcpy(header->payload,&input[12],length);
}

/* Empty a DNSHeader class out into raw data, ready for transmission */
inline void DNS::EmptyHeader(unsigned char *output, const DNSHeader *header, const int length)
{
	output[0] = header->id[0];
	output[1] = header->id[1];
	output[2] = header->flags1;
	output[3] = header->flags2;
	output[4] = header->qdcount >> 8;
	output[5] = header->qdcount & 0xFF;
	output[6] = header->ancount >> 8;
	output[7] = header->ancount & 0xFF;
	output[8] = header->nscount >> 8;
	output[9] = header->nscount & 0xFF;
	output[10] = header->arcount >> 8;
	output[11] = header->arcount & 0xFF;
	memcpy(&output[12],header->payload,length);
}

/* Send requests we have previously built down the UDP socket */
int DNSRequest::SendRequests(const DNSHeader *header, const int length, QueryType qt)
{
	insp_sockaddr addr;
	unsigned char payload[sizeof(DNSHeader)];

	this->rr_class = 1;
	this->type = qt;
		
	DNS::EmptyHeader(payload,header,length);

	memset(&addr,0,sizeof(addr));
#ifdef IPV6
	memcpy(&addr.sin6_addr,&myserver,sizeof(addr.sin6_addr));
	addr.sin6_family = AF_FAMILY;
	addr.sin6_port = htons(53);
#else
	memcpy(&addr.sin_addr.s_addr,&myserver,sizeof(addr.sin_addr));
	addr.sin_family = AF_FAMILY;
	addr.sin_port = htons(53);
#endif
	if (sendto(DNS::GetMasterSocket(), payload, length + 12, 0, (sockaddr *) &addr, sizeof(addr)) == -1)
	{
		log(DEBUG,"Error in sendto!");
		return -1;
	}

	return 0;
}

/* Add a query with a predefined header, and allocate an ID for it. */
DNSRequest* DNS::AddQuery(DNSHeader *header, int &id)
{
	id = this->PRNG() & 0xFFFF;

	/* This id is already 'in flight', pick another.
	 * -- Thanks jilles 
	 */
	while (requests.find(id) != requests.end())
		id = this->PRNG() & 0xFFFF;

	DNSRequest* req = new DNSRequest(this->myserver);

	header->id[0] = req->id[0] = id >> 8;
	header->id[1] = req->id[1] = id & 0xFF;
	header->flags1 = FLAGS_MASK_RD;
	header->flags2 = 0;
	header->qdcount = 1;
	header->ancount = 0;
	header->nscount = 0;
	header->arcount = 0;

	/* At this point we already know the id doesnt exist,
	 * so there needs to be no second check for the ::end()
	 */
	requests[id] = req;

	/* According to the C++ spec, new never returns NULL. */
	return req;
}

int DNS::GetMasterSocket()
{
	return MasterSocket;
}

/* Initialise the DNS UDP socket so that we can send requests */
DNS::DNS()
{
	insp_inaddr addr;

	/* Clear the Resolver class table */
	memset(Classes,0,sizeof(Classes));

	/* Set the id of the next request to 0
	 */
	currid = 0;

	/* Clear the namesever address */
	memset(&myserver,0,sizeof(insp_inaddr));

	/* Convert the nameserver address into an insp_inaddr */
	if (insp_aton(Config->DNSServer,&addr) > 0)
		memcpy(&myserver,&addr,sizeof(insp_inaddr));

	/* Initialize mastersocket */
	MasterSocket = socket(PF_PROTOCOL, SOCK_DGRAM, 0);
	if (MasterSocket != -1)
	{
		/* Did it succeed? */
		if (fcntl(MasterSocket, F_SETFL, O_NONBLOCK) != 0)
		{
			/* Couldn't make the socket nonblocking */
			shutdown(MasterSocket,2);
			close(MasterSocket);
			MasterSocket = -1;
		}
	}
	/* Have we got a socket and is it nonblocking? */
	if (MasterSocket != -1)
	{
#ifdef IPV6
		insp_sockaddr addr;
		memset(&addr,0,sizeof(addr));
		addr.sin6_family = AF_FAMILY;
		addr.sin6_port = 0;
		memset(&addr.sin6_addr,255,sizeof(in6_addr));
#else
		insp_sockaddr addr;
		memset(&addr,0,sizeof(addr));
		addr.sin_family = AF_FAMILY;
		addr.sin_port = 0;
		addr.sin_addr.s_addr = INADDR_ANY;
#endif
		/* Bind the port */
		if (bind(MasterSocket,(sockaddr *)&addr,sizeof(addr)) != 0)
		{
			/* Failed to bind */
			log(DEBUG,"Cant bind DNS::MasterSocket");
			shutdown(MasterSocket,2);
			close(MasterSocket);
			MasterSocket = -1;
		}

		if (MasterSocket >= 0)
		{
			/* Hook the descriptor into the socket engine */
			if (ServerInstance && ServerInstance->SE)
				ServerInstance->SE->AddFd(MasterSocket,true,X_ESTAB_DNS);
		}
	}
}

/* Build a payload to be placed after the header, based upon input data, a resource type, a class and a pointer to a buffer */
int DNS::MakePayload(const char * const name, const QueryType rr, const unsigned short rr_class, unsigned char * const payload)
{
	short payloadpos = 0;
	const char* tempchr, *tempchr2 = name;
	unsigned short length;

	/* split name up into labels, create query */
	while ((tempchr = strchr(tempchr2,'.')) != NULL)
	{
		length = tempchr - tempchr2;
		if (payloadpos + length + 1 > 507)
			return -1;
		payload[payloadpos++] = length;
		memcpy(&payload[payloadpos],tempchr2,length);
		payloadpos += length;
		tempchr2 = &tempchr[1];
	}
	length = strlen(tempchr2);
	if (length)
	{
		if (payloadpos + length + 2 > 507)
			return -1;
		payload[payloadpos++] = length;
		memcpy(&payload[payloadpos],tempchr2,length);
		payloadpos += length;
		payload[payloadpos++] = 0;
	}
	if (payloadpos > 508)
		return -1;
	length = htons(rr);
	memcpy(&payload[payloadpos],&length,2);
	length = htons(rr_class);
	memcpy(&payload[payloadpos + 2],&length,2);
	return payloadpos + 4;
}

/* Start lookup of an hostname to an IP address */
int DNS::GetIP(const char *name)
{
	DNSHeader h;
	int id;
	int length;
	
	if ((length = this->MakePayload(name, DNS_QUERY_A, 1, (unsigned char*)&h.payload)) == -1)
		return -1;

	DNSRequest* req = this->AddQuery(&h, id);

	if (req->SendRequests(&h, length, DNS_QUERY_A) == -1)
		return -1;

	return id;
}

/* Start lookup of an IP address to a hostname */
int DNS::GetName(const insp_inaddr *ip)
{
#ifdef IPV6
	return -1;
#else
	char query[29];
	DNSHeader h;
	int id;
	int length;

	unsigned char* c = (unsigned char*)&ip->s_addr;

	sprintf(query,"%d.%d.%d.%d.in-addr.arpa",c[3],c[2],c[1],c[0]);

	if ((length = this->MakePayload(query, DNS_QUERY_PTR, 1, (unsigned char*)&h.payload)) == -1)
		return -1;

	DNSRequest* req = this->AddQuery(&h, id);

	if (req->SendRequests(&h, length, DNS_QUERY_PTR) == -1)
		return -1;

	return id;
#endif
}

/* Return the next id which is ready, and the result attached to it */
DNSResult DNS::GetResult()
{
	/* Fetch dns query response and decide where it belongs */
	DNSHeader header;
	DNSRequest *req;
	unsigned char buffer[sizeof(DNSHeader)];

	/* Attempt to read a header */
	int length = recv(MasterSocket,buffer,sizeof(DNSHeader),0);

	/* Did we get the whole header? */
	if (length < 12)
		/* Nope - something screwed up. */
		return std::make_pair(-1,"");

	/* Put the read header info into a header class */
	DNS::FillHeader(&header,buffer,length - 12);

	/* Get the id of this request.
	 * Its a 16 bit value stored in two char's,
	 * so we use logic shifts to create the value.
	 */
	unsigned long this_id = header.id[1] + (header.id[0] << 8);

	/* Do we have a pending request matching this id? */
	requestlist_iter n_iter = requests.find(this_id);
	if (n_iter == requests.end())
	{
		/* Somehow we got a DNS response for a request we never made... */
		log(DEBUG,"DNS: got a response for a query we didnt send with fd=%d queryid=%d",MasterSocket,this_id);
		return std::make_pair(-1,"");
	}
	else
	{
		/* Remove the query from the list of pending queries */
		req = (DNSRequest*)n_iter->second;
		requests.erase(n_iter);
	}

	/* Inform the DNSRequest class that it has a result to be read.
	 * When its finished it will return a DNSInfo which is a pair of
	 * unsigned char* resource record data, and an error message.
	 */
	DNSInfo data = req->ResultIsReady(header, length);
	std::string resultstr;

	/* Check if we got a result, if we didnt, its an error */
	if (data.first == NULL)
	{
		/* An error.
		 * Mask the ID with the value of ERROR_MASK, so that
		 * the dns_deal_with_classes() function knows that its
		 * an error response and needs to be treated uniquely.
		 * Put the error message in the second field.
		 */
		delete req;
		return std::make_pair(this_id | ERROR_MASK, data.second);
	}
	else
	{
		/* Forward lookups come back as binary data. We must format them into ascii */
		if (req->type == DNS_QUERY_A)
		{
			char formatted[16];
			snprintf(formatted,16,"%u.%u.%u.%u",data.first[0],data.first[1],data.first[2],data.first[3]);
			resultstr = formatted;
		}
		else
		{
			/* Reverse lookups just come back as char* */
			resultstr = std::string((const char*)data.first);
		}

		/* Build the reply with the id and hostname/ip in it */
		delete req;
		return std::make_pair(this_id,resultstr);
	}
}

/* A result is ready, process it */
DNSInfo DNSRequest::ResultIsReady(DNSHeader &header, int length)
{
	int i = 0;
	int q = 0;
	int curanswer, o;
	ResourceRecord rr;
 	unsigned short ptr;
			
	if (!(header.flags1 & FLAGS_MASK_QR))
		return std::make_pair((unsigned char*)NULL,"Not a query result");

	if (header.flags1 & FLAGS_MASK_OPCODE)
		return std::make_pair((unsigned char*)NULL,"Unexpected value in DNS reply packet");

	if (header.flags2 & FLAGS_MASK_RCODE)
		return std::make_pair((unsigned char*)NULL,"Domain name not found");

	if (header.ancount < 1)
		return std::make_pair((unsigned char*)NULL,"No resource records returned");

	/* Subtract the length of the header from the length of the packet */
	length -= 12;

	while ((unsigned int)q < header.qdcount && i < length)
	{
		if (header.payload[i] > 63)
		{
			i += 6;
			q++;
		}
		else
		{
			if (header.payload[i] == 0)
			{
				q++;
				i += 5;
			}
			else i += header.payload[i] + 1;
		}
	}
	curanswer = 0;
	while ((unsigned)curanswer < header.ancount)
	{
		q = 0;
		while (q == 0 && i < length)
		{
			if (header.payload[i] > 63)
			{
				i += 2;
				q = 1;
			}
			else
			{
				if (header.payload[i] == 0)
				{
					i++;
					q = 1;
				}
				else i += header.payload[i] + 1; /* skip length and label */
			}
		}
		if (length - i < 10)
			return std::make_pair((unsigned char*)NULL,"Incorrectly sized DNS reply");

		DNS::FillResourceRecord(&rr,&header.payload[i]);
		i += 10;
		if (rr.type != this->type)
		{
			curanswer++;
			i += rr.rdlength;
			continue;
		}
		if (rr.rr_class != this->rr_class)
		{
			curanswer++;
			i += rr.rdlength;
			continue;
		}
		break;
	}
	if ((unsigned int)curanswer == header.ancount)
		return std::make_pair((unsigned char*)NULL,"No valid answers");

	if (i + rr.rdlength > (unsigned int)length)
		return std::make_pair((unsigned char*)NULL,"Resource record larger than stated");

	if (rr.rdlength > 1023)
		return std::make_pair((unsigned char*)NULL,"Resource record too large");

	switch (rr.type)
	{
		case DNS_QUERY_PTR:
			o = 0;
			q = 0;
			while (q == 0 && i < length && o + 256 < 1023)
			{
				if (header.payload[i] > 63)
				{
					memcpy(&ptr,&header.payload[i],2);
					i = ntohs(ptr) - 0xC000 - 12;
				}
				else
				{
					if (header.payload[i] == 0)
					{
						q = 1;
					}
					else
					{
						res[o] = 0;
						if (o != 0)
							res[o++] = '.';
						memcpy(&res[o],&header.payload[i + 1],header.payload[i]);
						o += header.payload[i];
						i += header.payload[i] + 1;
					}
				}
			}
			res[o] = 0;
		break;
		case DNS_QUERY_A:
			memcpy(res,&header.payload[i],rr.rdlength);
			res[rr.rdlength] = 0;
			break;
		default:
			memcpy(res,&header.payload[i],rr.rdlength);
			res[rr.rdlength] = 0;
			break;
	}
	return std::make_pair(res,"No error");;
}

/* Close the master socket */
DNS::~DNS()
{
	shutdown(MasterSocket, 2);
	close(MasterSocket);
}

/* High level abstraction of dns used by application at large */
Resolver::Resolver(const std::string &source, bool forward) : input(source), fwd(forward)
{
	if (forward)
	{
		log(DEBUG,"Resolver: Forward lookup on %s",source.c_str());
		this->myid = ServerInstance->Res->GetIP(source.c_str());
	}
	else
	{
		log(DEBUG,"Resolver: Reverse lookup on %s",source.c_str());
		insp_inaddr binip;
	        if (insp_aton(source.c_str(), &binip) > 0)
		{
			/* Valid ip address */
	        	this->myid = ServerInstance->Res->GetName(&binip);
		}
		else
		{
			this->OnError(RESOLVER_BADIP, "Bad IP address for reverse lookup");
			throw ModuleException("Resolver: Bad IP address");
			return;
		}
	}
	if (this->myid == -1)
	{
		log(DEBUG,"Resolver::Resolver: Could not get an id!");
		this->OnError(RESOLVER_NSDOWN, "Nameserver is down");
		throw ModuleException("Resolver: Couldnt get an id to make a request");
		/* We shouldnt get here really */
		return;
	}

	log(DEBUG,"Resolver::Resolver: this->myid=%d",this->myid);
}

void Resolver::OnError(ResolverError e, const std::string &errormessage)
{
	/* Nothing in here */
}

Resolver::~Resolver()
{
	/* Nothing here (yet) either */
}

/* Get the request id associated with this class */
int Resolver::GetId()
{
	return this->myid;
}

/* Process a socket read event */
void DNS::MarshallReads(int fd)
{
	/* We are only intrested in our single fd */
	if (fd == GetMasterSocket())
	{
		/* Fetch the id and result of the next available packet */
		DNSResult res = this->GetResult();
		/* Is there a usable request id? */
		if (res.first != -1)
		{
			/* Its an error reply */
			if (res.first & ERROR_MASK)
			{
				/* Mask off the error bit */
				res.first -= ERROR_MASK;

				/* Marshall the error to the correct class */
				log(DEBUG,"Error available, id=%d",res.first);
				if (Classes[res.first])
				{
					if (ServerInstance && ServerInstance->stats)
						ServerInstance->stats->statsDnsBad++;
					Classes[res.first]->OnError(RESOLVER_NXDOMAIN, res.second);
					delete Classes[res.first];
					Classes[res.first] = NULL;
				}
			}
			else
			{
				/* It is a non-error result */
				log(DEBUG,"Result available, id=%d",res.first);
				/* Marshall the result to the correct class */
				if (Classes[res.first])
				{
					if (ServerInstance && ServerInstance->stats)
						ServerInstance->stats->statsDnsGood++;
					Classes[res.first]->OnLookupComplete(res.second);
					delete Classes[res.first];
					Classes[res.first] = NULL;
				}
			}

			if (ServerInstance && ServerInstance->stats)
				ServerInstance->stats->statsDns++;

		}
	}
}

/* Add a derived Resolver to the working set */
bool DNS::AddResolverClass(Resolver* r)
{
	/* Check the pointers validity and the id's validity */
	if ((r) && (r->GetId() > -1))
	{
		/* Check the slot isnt already occupied -
		 * This should NEVER happen unless we have
		 * a severely broken DNS server somewhere
		 */
		if (!Classes[r->GetId()])
		{
			/* Set up the pointer to the class */
			Classes[r->GetId()] = r;
			return true;
		}
		else
			/* Duplicate id */
			return false;
	}
	else
	{
		/* Pointer or id not valid.
		 * Free the item and return
		 */
		if (r)
			delete r;

		return false;
	}
}

unsigned long DNS::PRNG()
{
	unsigned long val = 0;
	timeval n;
	serverstats* s = ServerInstance->stats;
	gettimeofday(&n,NULL);
	val = (n.tv_usec ^ getpid() ^ geteuid() ^ (this->currid++)) ^ s->statsAccept + n.tv_sec;
	val = val + s->statsCollisions ^ s->statsDnsGood - s->statsDnsBad;
	val += (s->statsConnects ^ (unsigned long)s->statsSent ^ (unsigned long)s->statsRecv) - s->BoundPortCount;
	return val;
}

