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
Very loosely based on the firedns library,
Copyright (C) 2002 Ian Gulliver.

There have been so many modifications to this file
to make it fit into InspIRCd and make it object
orientated that you should not take this code as
being what firedns really looks like. It used to
look very different to this! :-P
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

extern InspIRCd* ServerInstance;
extern ServerConfig* Config;
extern time_t TIME;
extern userrec* fd_ref_table[MAX_DESCRIPTORS];

enum QueryType
{
	DNS_QRY_A	= 1,
	DNS_QRY_PTR	= 12
};

enum QueryInfo
{
	ERROR_MASK	= 0x10000
};

enum QueryFlags
{
	FLAGS_MASK_RD		= 0x01,
	FLAGS_MASK_TC		= 0x02,
	FLAGS_MASK_AA		= 0x04,
	FLAGS_MASK_OPCODE	= 0x78,
	FLAGS_MASK_QR		= 0x80,
	FLAGS_MASK_RCODE	= 0x0F,
	FLAGS_MASK_Z		= 0x70,
	FLAGS_MASK_RA 		= 0x80
};

class dns_connection;
typedef std::map<int,dns_connection*> connlist;
typedef connlist::iterator connlist_iter;

DNS* Res = NULL;

connlist connections;
int master_socket = -1;
Resolver* dns_classes[65536];
insp_inaddr myserver;

class dns_rr_middle
{
 public:
	QueryType	type;
	unsigned int	rr_class;
	unsigned long	ttl;
	unsigned int	rdlength;
};

class dns_header
{
 public:
	unsigned char	id[2];
	unsigned int	flags1;
	unsigned int	flags2;
	unsigned int	qdcount;
	unsigned int	ancount;
	unsigned int	nscount;
	unsigned int	arcount;
	unsigned char	payload[512];
};

class dns_connection
{
 public:
	unsigned char   id[2];
	unsigned char*	res;
	unsigned int    rr_class;
	QueryType       type;

	dns_connection()
	{
		res = new unsigned char[512];
		*res = 0;
	}

	~dns_connection()
	{
		delete[] res;
	}

	DNSInfo	result_ready(dns_header &h, int length);
	int	send_requests(const dns_header *header, const int length, QueryType qt);
};

/*
 * Optimized by brain, these were using integer division and modulus.
 * We can use logic shifts and logic AND to replace these even divisions
 * and multiplications, it should be a bit faster (probably not noticably,
 * but of course, more impressive). Also made these inline.
 */

inline void dns_fill_rr(dns_rr_middle* rr, const unsigned char *input)
{
	rr->type = (QueryType)((input[0] << 8) + input[1]);
	rr->rr_class = (input[2] << 8) + input[3];
	rr->ttl = (input[4] << 24) + (input[5] << 16) + (input[6] << 8) + input[7];
	rr->rdlength = (input[8] << 8) + input[9];
}

inline void dns_fill_header(dns_header *header, const unsigned char *input, const int length)
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

inline void dns_empty_header(unsigned char *output, const dns_header *header, const int length)
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


int dns_connection::send_requests(const dns_header *header, const int length, QueryType qt)
{
	insp_sockaddr addr;
	unsigned char payload[sizeof(dns_header)];

	this->rr_class = 1;
	this->type = qt;
		
	dns_empty_header(payload,header,length);

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
	if (sendto(master_socket, payload, length + 12, 0, (sockaddr *) &addr, sizeof(addr)) == -1)
	{
		log(DEBUG,"Error in sendto!");
		return -1;
	}

	return 0;
}

dns_connection* dns_add_query(dns_header *header, int &id)
{

	id = rand() % 65536;
	dns_connection* req = new dns_connection();

	header->id[0] = req->id[0] = id >> 8;
	header->id[1] = req->id[1] = id & 0xFF;
	header->flags1 = FLAGS_MASK_RD;
	header->flags2 = 0;
	header->qdcount = 1;
	header->ancount = 0;
	header->nscount = 0;
	header->arcount = 0;

	if (connections.find(id) == connections.end())
		connections[id] = req;

	/* According to the C++ spec, new never returns NULL. */
	return req;
}

void create_socket()
{
	log(DEBUG,"---- BEGIN DNS INITIALIZATION, SERVER=%s ---",Config->DNSServer);
	insp_inaddr addr;
	srand((unsigned int) TIME);
	memset(&myserver,0,sizeof(insp_inaddr));
	if (insp_aton(Config->DNSServer,&addr) > 0)
		memcpy(&myserver,&addr,sizeof(insp_inaddr));

	master_socket = socket(PF_PROTOCOL, SOCK_DGRAM, 0);
	if (master_socket != -1)
	{
		log(DEBUG,"Set query socket nonblock");
		if (fcntl(master_socket, F_SETFL, O_NONBLOCK) != 0)
		{
			shutdown(master_socket,2);
			close(master_socket);
			master_socket = -1;
		}
	}
	if (master_socket != -1)
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
		log(DEBUG,"Binding query port");
		if (bind(master_socket,(sockaddr *)&addr,sizeof(addr)) != 0)
		{
			log(DEBUG,"Cant bind with source port = 0");
			shutdown(master_socket,2);
			close(master_socket);
			master_socket = -1;
		}

		if (master_socket >= 0)
		{
			log(DEBUG,"Attach query port to socket engine");
			if (ServerInstance && ServerInstance->SE)
				ServerInstance->SE->AddFd(master_socket,true,X_ESTAB_DNS);
		}
	}
}

int dns_build_query_payload(const char * const name, const unsigned short rr, const unsigned short rr_class, unsigned char * const payload)
{
	short payloadpos;
	const char * tempchr, * tempchr2;
	unsigned short l;

	payloadpos = 0;
	tempchr2 = name;

	/* split name up into labels, create query */
	while ((tempchr = strchr(tempchr2,'.')) != NULL)
	{
		l = tempchr - tempchr2;
		if (payloadpos + l + 1 > 507)
			return -1;
		payload[payloadpos++] = l;
		memcpy(&payload[payloadpos],tempchr2,l);
		payloadpos += l;
		tempchr2 = &tempchr[1];
	}
	l = strlen(tempchr2);
	if (l)
	{
		if (payloadpos + l + 2 > 507)
			return -1;
		payload[payloadpos++] = l;
		memcpy(&payload[payloadpos],tempchr2,l);
		payloadpos += l;
		payload[payloadpos++] = '\0';
	}
	if (payloadpos > 508)
		return -1;
	l = htons(rr);
	memcpy(&payload[payloadpos],&l,2);
	l = htons(rr_class);
	memcpy(&payload[payloadpos + 2],&l,2);
	return payloadpos + 4;
}

int DNS::dns_getip(const char *name)
{
	dns_header h;
	int id;
	int length;
	dns_connection* req;
	
	if ((length = dns_build_query_payload(name,DNS_QRY_A,1,(unsigned char*)&h.payload)) == -1)
		return -1;

	req = dns_add_query(&h, id);

	if (req->send_requests(&h,length,DNS_QRY_A) == -1)
		return -1;

	return id;
}

int DNS::dns_getname(const insp_inaddr *ip)
{
#ifdef IPV6
	return -1;
#else
	char query[29];
	dns_header h;
	int id;
	int length;
	dns_connection* req;

	unsigned char* c = (unsigned char*)&ip->s_addr;

	sprintf(query,"%d.%d.%d.%d.in-addr.arpa",c[3],c[2],c[1],c[0]);

	if ((length = dns_build_query_payload(query,DNS_QRY_PTR,1,(unsigned char*)&h.payload)) == -1)
		return -1;

	req = dns_add_query(&h, id);

	if (req->send_requests(&h,length,DNS_QRY_PTR) == -1)
		return -1;

	return id;
#endif
}

/* Return the next id which is ready, and the result attached to it
 */
DNSResult DNS::dns_getresult()
{
	/* retrieve result of DNS query (buffered) */
	dns_header header;
	dns_connection *req;
	int length;
	unsigned char buffer[sizeof(dns_header)];

	length = recv(master_socket,buffer,sizeof(dns_header),0);

	if (length < 12)
		return std::make_pair(-1,"");

	dns_fill_header(&header,buffer,length - 12);

	// Get the id of this request
	unsigned long this_id = header.id[1] + (header.id[0] << 8);

	// Do we have a pending request for it?

        connlist_iter n_iter = connections.find(this_id);
        if (n_iter == connections.end())
        {
                log(DEBUG,"DNS: got a response for a query we didnt send with fd=%d queryid=%d",master_socket,this_id);
                return std::make_pair(-1,"");
        }
        else
        {
		/* Remove the query from the list */
		req = (dns_connection*)n_iter->second;
		/* We don't delete c here, because its done later when needed */
		connections.erase(n_iter);
        }
	DNSInfo data = req->result_ready(header, length);
	std::string resultstr;

	if (data.first == NULL)
	{
		delete req;
		return std::make_pair(this_id | ERROR_MASK, data.second);
	}
	else
	{
		if (req->type == DNS_QRY_A)
		{
			char formatted[16];
			snprintf(formatted,16,"%u.%u.%u.%u",data.first[0],data.first[1],data.first[2],data.first[3]);
			resultstr = formatted;
		}
		else
		{
			resultstr = std::string((const char*)data.first);
		}

		delete req;
		return std::make_pair(this_id,resultstr);
	}
}

/** A result is ready, process it
 */
DNSInfo dns_connection::result_ready(dns_header &header, int length)
{
	int i = 0;
	int q = 0;
	int curanswer, o;
	dns_rr_middle rr;
 	unsigned short p;
					
	if (!(header.flags1 & FLAGS_MASK_QR))
		return std::make_pair((unsigned char*)NULL,"Not a query result");

	if (header.flags1 & FLAGS_MASK_OPCODE)
		return std::make_pair((unsigned char*)NULL,"Unexpected value in DNS reply packet");

	if (header.flags2 & FLAGS_MASK_RCODE)
		return std::make_pair((unsigned char*)NULL,"Internal server error (SERVFAIL)");

	if (header.ancount < 1)
		return std::make_pair((unsigned char*)NULL,"No resource records returned");

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

		dns_fill_rr(&rr,&header.payload[i]);
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
		case DNS_QRY_PTR:
			o = 0;
			q = 0;
			while (q == 0 && i < length && o + 256 < 1023)
			{
				if (header.payload[i] > 63)
				{
					memcpy(&p,&header.payload[i],2);
					i = ntohs(p) - 0xC000 - 12;
				}
				else
				{
					if (header.payload[i] == 0)
					{
						q = 1;
					}
					else
					{
						res[o] = '\0';
						if (o != 0)
							res[o++] = '.';
						memcpy(&res[o],&header.payload[i + 1],header.payload[i]);
						o += header.payload[i];
						i += header.payload[i] + 1;
					}
				}
			}
			res[o] = '\0';
		break;
		case DNS_QRY_A:
			memcpy(res,&header.payload[i],rr.rdlength);
			res[rr.rdlength] = '\0';
			break;
		default:
			memcpy(res,&header.payload[i],rr.rdlength);
			res[rr.rdlength] = '\0';
			break;
	}
	return std::make_pair(res,"No error");;
}

DNS::DNS()
{
}

DNS::~DNS()
{
}

Resolver::Resolver(const std::string &source, bool forward) : input(source), fwd(forward)
{
	if (forward)
	{
		log(DEBUG,"Resolver: Forward lookup on %s",source.c_str());
		this->myid = Res->dns_getip(source.c_str());
	}
	else
	{
		log(DEBUG,"Resolver: Reverse lookup on %s",source.c_str());
		insp_inaddr binip;
	        if (insp_aton(source.c_str(), &binip) > 0)
		{
			/* Valid ip address */
	        	this->myid = Res->dns_getname(&binip);
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

//void Resolver::OnLookupComplete(const std::string &result)
//{
//}

void Resolver::OnError(ResolverError e, const std::string &errormessage)
{
}

Resolver::~Resolver()
{
	log(DEBUG,"Resolver::~Resolver");
}

int Resolver::GetId()
{
	return this->myid;
}

void dns_deal_with_classes(int fd)
{
	log(DEBUG,"dns_deal_with_classes(%d)",fd);
	if (fd == master_socket)
	{
		DNSResult res = Res->dns_getresult();
		if (res.first != -1)
		{
			if (res.first & ERROR_MASK)
			{
				res.first -= ERROR_MASK;

				log(DEBUG,"Error available, id=%d",res.first);
				if (dns_classes[res.first])
				{
					dns_classes[res.first]->OnError(RESOLVER_NXDOMAIN, res.second);
					delete dns_classes[res.first];
					dns_classes[res.first] = NULL;
				}
			}
			else
			{
				log(DEBUG,"Result available, id=%d",res.first);
				if (dns_classes[res.first])
				{
					dns_classes[res.first]->OnLookupComplete(res.second);
					delete dns_classes[res.first];
					dns_classes[res.first] = NULL;
				}
			}
		}
	}
}

bool dns_add_class(Resolver* r)
{
	log(DEBUG,"dns_add_class");
	if ((r) && (r->GetId() > -1))
	{
		if (!dns_classes[r->GetId()])
		{
			log(DEBUG,"dns_add_class: added class");
			dns_classes[r->GetId()] = r;
			return true;
		}
		else
		{
			log(DEBUG,"Space occupied!");
			return false;
		}
	}
	else
	{
		log(DEBUG,"Bad class");
		delete r;
		return true;
	}
}

void init_dns()
{
	Res = new DNS();
	memset(dns_classes,0,sizeof(dns_classes));
	create_socket();
}

