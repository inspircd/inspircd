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
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <map>
#include <algorithm>
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

enum QueryType { DNS_QRY_A = 1, DNS_QRY_PTR = 12 };
enum QueryFlags1 { FLAGS1_MASK_RD = 0x01, FLAGS1_MASK_TC = 0x02, FLAGS1_MASK_AA = 0x04, FLAGS1_MASK_OPCODE = 0x78, FLAGS1_MASK_QR = 0x80 };
enum QueryFlags2 { FLAGS2_MASK_RCODE = 0x0F, FLAGS2_MASK_Z = 0x70, FLAGS2_MASK_RA = 0x80 };

class s_connection;

typedef std::map<int,s_connection*> connlist;
typedef connlist::iterator connlist_iter;

DNS* Res = NULL;

connlist connections;
int master_socket = -1;
Resolver* dns_classes[65536];
insp_inaddr myserver;

class s_rr_middle
{
 public:
	QueryType	type;
	unsigned int	_class;
	unsigned long	ttl;
	unsigned int	rdlength;
};

class s_header
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

class s_connection
{
 public:
	unsigned char   id[2];
	unsigned char	res[512];
	unsigned int    _class;
	QueryType       type;

	s_connection()
	{
		*res = 0;
	}

	unsigned char*	result_ready(s_header &h, int length);
	int		send_requests(const s_header *h, const int l, QueryType qt);
};



void *dns_align(void *inp)
{
	char *p = (char*)inp;
	int offby = ((char *)p - (char *)0) % (sizeof(void *) > sizeof(long) ? sizeof(void *) : sizeof(long));
	if (offby != 0)
		return p + ((sizeof(void *) > sizeof(long) ? sizeof(void *) : sizeof(long)) - offby);
	else
		return p;
}

/*
 * Optimized by brain, these were using integer division and modulus.
 * We can use logic shifts and logic AND to replace these even divisions
 * and multiplications, it should be a bit faster (probably not noticably,
 * but of course, more impressive). Also made these inline.
 */

inline void dns_fill_rr(s_rr_middle* rr, const unsigned char *input)
{
	rr->type = (QueryType)((input[0] << 8) + input[1]);
	rr->_class = (input[2] << 8) + input[3];
	rr->ttl = (input[4] << 24) + (input[5] << 16) + (input[6] << 8) + input[7];
	rr->rdlength = (input[8] << 8) + input[9];
}

inline void dns_fill_header(s_header *header, const unsigned char *input, const int l)
{
	header->id[0] = input[0];
	header->id[1] = input[1];
	header->flags1 = input[2];
	header->flags2 = input[3];
	header->qdcount = (input[4] << 8) + input[5];
	header->ancount = (input[6] << 8) + input[7];
	header->nscount = (input[8] << 8) + input[9];
	header->arcount = (input[10] << 8) + input[11];
	memcpy(header->payload,&input[12],l);
}

inline void dns_empty_header(unsigned char *output, const s_header *header, const int l)
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
	memcpy(&output[12],header->payload,l);
}


int s_connection::send_requests(const s_header *h, const int l, QueryType qt)
{
	insp_sockaddr addr;
	unsigned char payload[sizeof(s_header)];

	this->_class = 1;
	this->type = qt;
		
	dns_empty_header(payload,h,l);

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
	if (sendto(master_socket, payload, l + 12, 0, (sockaddr *) &addr, sizeof(addr)) == -1)
	{
		log(DEBUG,"Error in sendto!");
		return -1;
	}

	return 0;
}

s_connection* dns_add_query(s_header *h, int &id)
{

	id = rand() % 65536;
	s_connection * s = new s_connection();

	h->id[0] = s->id[0] = id >> 8;
	h->id[1] = s->id[1] = id & 0xFF;
	h->flags1 = 0 | FLAGS1_MASK_RD;
	h->flags2 = 0;
	h->qdcount = 1;
	h->ancount = 0;
	h->nscount = 0;
	h->arcount = 0;

	if (connections.find(id) == connections.end())
		connections[id] = s;
	return s;
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

int dns_build_query_payload(const char * const name, const unsigned short rr, const unsigned short _class, unsigned char * const payload)
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
	l = htons(_class);
	memcpy(&payload[payloadpos + 2],&l,2);
	return payloadpos + 4;
}

int DNS::dns_getip4(const char *name)
{
	s_header h;
	int id;
	
	int length = dns_build_query_payload(name,DNS_QRY_A,1,(unsigned char*)&h.payload);
	if (length == -1)
		return -1;
	s_connection* req = dns_add_query(&h, id);
	if (req == NULL)
		return -1;

	if (req->send_requests(&h,length,DNS_QRY_A) == -1)
		return -1;

	return id;
}

int DNS::dns_getname4(const insp_inaddr *ip)
{
#ifdef IPV6
	return -1;
#else
	char query[29];
	s_header h;
	int id;

	unsigned char* c = (unsigned char*)&ip->s_addr;

	sprintf(query,"%d.%d.%d.%d.in-addr.arpa",c[3],c[2],c[1],c[0]);

	int length = dns_build_query_payload(query,DNS_QRY_PTR,1,(unsigned char*)&h.payload);
	if (length == -1)
		return -1;
	s_connection* req = dns_add_query(&h, id);
	if (req == NULL)
		return -1;
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
	s_header h;
	s_connection *c;
	int length;
	unsigned char buffer[sizeof(s_header)];

	length = recv(master_socket,buffer,sizeof(s_header),0);

	if (length < 12)
		return std::make_pair(-1,"");

	dns_fill_header(&h,buffer,length - 12);

	// Get the id of this request
	unsigned long this_id = h.id[1] + (h.id[0] << 8);

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
                c = (s_connection*)n_iter->second;
                /* We don't delete c here, because its done later when needed */
                connections.erase(n_iter);
        }
	unsigned char* a = c->result_ready(h, length);
	std::string resultstr;

	if (a == NULL)
	{
		resultstr = "";
	}
	else
	{
		if (c->type == DNS_QRY_A)
		{
			char formatted[1024];
			snprintf(formatted,1024,"%u.%u.%u.%u",a[0],a[1],a[2],a[3]);
			resultstr = std::string(formatted);
		}
		else
		{
			resultstr = std::string((const char*)a);
		}
	}

	delete c;
	return std::make_pair(this_id,resultstr);
}

/** A result is ready, process it
 */
unsigned char* s_connection::result_ready(s_header &h, int length)
{
	int i, q, curanswer, o;
	s_rr_middle rr;
 	unsigned short p;
					
	if ((h.flags1 & FLAGS1_MASK_QR) == 0)
	{
		log(DEBUG,"DNS: didnt get a query result");
		return NULL;
	}
	if ((h.flags1 & FLAGS1_MASK_OPCODE) != 0)
	{
		log(DEBUG,"DNS: got an OPCODE and didnt want one");
		return NULL;
	}
	if ((h.flags2 & FLAGS2_MASK_RCODE) != 0)
	{
		log(DEBUG,"DNS lookup failed due to SERVFAIL");
		return NULL;
	}
	if (h.ancount < 1)
	{
		log(DEBUG,"DNS: no answers!");
		return NULL;
	}
	i = 0;
	q = 0;
	length -= 12;
	while ((unsigned)q < h.qdcount && i < length)
	{
		if (h.payload[i] > 63)
		{
			i += 6;
			q++;
		}
		else
		{
			if (h.payload[i] == 0)
			{
				q++;
				i += 5;
			}
			else i += h.payload[i] + 1;
		}
	}
	curanswer = 0;
	while ((unsigned)curanswer < h.ancount)
	{
		q = 0;
		while (q == 0 && i < length)
		{
			if (h.payload[i] > 63)
			{
				i += 2;
				q = 1;
			}
			else
			{
				if (h.payload[i] == 0)
				{
					i++;
					q = 1;
				}
				else i += h.payload[i] + 1; /* skip length and label */
			}
		}
		if (length - i < 10)
		{
			return NULL;
		}
		dns_fill_rr(&rr,&h.payload[i]);
		i += 10;
		if (rr.type != this->type)
		{
			curanswer++;
			i += rr.rdlength;
			continue;
		}
		if (rr._class != this->_class)
		{
			curanswer++;
			i += rr.rdlength;
			continue;
		}
		break;
	}
	if ((unsigned int)curanswer == h.ancount)
		return NULL;
	if (i + rr.rdlength > (unsigned int)length)
		return NULL;
	if (rr.rdlength > 1023)
		return NULL;

	switch (rr.type)
	{
		case DNS_QRY_PTR:
			log(DEBUG,"DNS: got a result of type DNS_QRY_PTR");
			o = 0;
			q = 0;
			while (q == 0 && i < length && o + 256 < 1023)
			{
				if (h.payload[i] > 63)
				{
					log(DEBUG,"DNS: h.payload[i] > 63");
					memcpy(&p,&h.payload[i],2);
					i = ntohs(p) - 0xC000 - 12;
				}
				else
				{
					if (h.payload[i] == 0)
					{
						q = 1;
					}
					else
					{
						res[o] = '\0';
						if (o != 0)
							res[o++] = '.';
						memcpy(&res[o],&h.payload[i + 1],h.payload[i]);
						o += h.payload[i];
						i += h.payload[i] + 1;
					}
				}
			}
			res[o] = '\0';
		break;
		case DNS_QRY_A:
			log(DEBUG,"DNS: got a result of type DNS_QRY_A");
			memcpy(res,&h.payload[i],rr.rdlength);
			res[rr.rdlength] = '\0';
			break;
		default:
			memcpy(res,&h.payload[i],rr.rdlength);
			res[rr.rdlength] = '\0';
			break;
	}
	return res;
}

DNS::DNS()
{
	log(DEBUG,"Create blank DNS");
}

DNS::~DNS()
{
}

Resolver::Resolver(const std::string &source, bool forward) : input(source), fwd(forward)
{
	if (forward)
	{
		log(DEBUG,"Resolver: Forward lookup on %s",source.c_str());
		this->myid = Res->dns_getip4(source.c_str());
	}
	else
	{
		log(DEBUG,"Resolver: Reverse lookup on %s",source.c_str());
		insp_inaddr binip;
	        if (insp_aton(source.c_str(), &binip) > 0)
		{
			/* Valid ip address */
	        	this->myid = Res->dns_getname4(&binip);
		}
	}
	if (this->myid == -1)
	{
		log(DEBUG,"Resolver::Resolver: Could not get an id!");
		this->OnError(RESOLVER_NSDOWN);
		throw ModuleException("Resolver: Couldnt get an id to make a request");
		/* We shouldnt get here really */
		return;
	}

	log(DEBUG,"Resolver::Resolver: this->myid=%d",this->myid);
}

void Resolver::OnLookupComplete(const std::string &result)
{
}

void Resolver::OnError(ResolverError e)
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

bool Resolver::ProcessResult(const std::string &result)
{
	log(DEBUG,"Resolver::ProcessResult");

	if (!result.length())
	{
		log(DEBUG,"Resolver::OnError(RESOLVER_NXDOMAIN)");
		this->OnError(RESOLVER_NXDOMAIN);
		return false;
	}
	else
	{

		log(DEBUG,"Resolver::OnLookupComplete(%s)",result.c_str());
		this->OnLookupComplete(result);
		return true;
	}
}

void dns_deal_with_classes(int fd)
{
	log(DEBUG,"dns_deal_with_classes(%d)",fd);
	if (fd == master_socket)
	{
		DNSResult res = Res->dns_getresult();
		if (res.first != -1)
		{
			log(DEBUG,"Result available, id=%d",res.first);
			if (dns_classes[res.first])
			{
				dns_classes[res.first]->ProcessResult(res.second);
				delete dns_classes[res.first];
				dns_classes[res.first] = NULL;
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

