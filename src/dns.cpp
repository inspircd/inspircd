/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
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

#ifdef THREADED_DNS
pthread_mutex_t connmap_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

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
connlist connections;

Resolver* dns_classes[MAX_DESCRIPTORS];

struct in_addr servers4[8];
int i4;
int initdone = 0;
int lastcreate = -1;

class s_connection
{
 public:
	unsigned char	id[2];
	unsigned int	_class;
	QueryType	type;
	int		want_list;
	int		fd;
};

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

void dns_close(int fd)
{
#ifndef THREADED_DNS
	if (ServerInstance && ServerInstance->SE)
		ServerInstance->SE->DelFd(fd);
#endif
	log(DEBUG,"DNS: dns_close on fd %d",fd);
	shutdown(fd,2);
	close(fd);
	return;
}

void DNS::dns_init()
{
	FILE *f;
	int i;
	in_addr addr4;
	char buf[1024];
	if (initdone == 1)
		return;
	i4 = 0;

	initdone = 1;
	srand((unsigned int) TIME);
	memset(servers4,'\0',sizeof(in_addr) * 8);
	f = fopen("/etc/resolv.conf","r");
	if (f == NULL)
		return;
	while (fgets(buf,1024,f) != NULL) {
		if (strncmp(buf,"nameserver",10) == 0)
		{
			i = 10;
			while (buf[i] == ' ' || buf[i] == '\t')
				i++;
			if (i4 < 8)
			{
				if (dns_aton4_s(&buf[i],&addr4) != NULL)
					memcpy(&servers4[i4++],&addr4,sizeof(in_addr));
			}
		}
	}
	fclose(f);
}

void DNS::dns_init_2(const char* dnsserver)
{
	in_addr addr4;
	i4 = 0;
	srand((unsigned int) TIME);
	memset(servers4,'\0',sizeof(in_addr) * 8);
	if (dns_aton4_s(dnsserver,&addr4) != NULL)
	    memcpy(&servers4[i4++],&addr4,sizeof(in_addr));
}


int dns_send_requests(const s_header *h, const s_connection *s, const int l)
{
	int i;
	sockaddr_in addr4;
	unsigned char payload[sizeof(s_header)];

	dns_empty_header(payload,h,l);


	i = 0;

	/* otherwise send via standard ipv4 boringness */
	memset(&addr4,0,sizeof(addr4));
	memcpy(&addr4.sin_addr,&servers4[i],sizeof(addr4.sin_addr));
	addr4.sin_family = AF_INET;
	addr4.sin_port = htons(53);
	if (sendto(s->fd, payload, l + 12, 0, (sockaddr *) &addr4, sizeof(addr4)) == -1)
	{
		return -1;
	}

	return 0;
}

s_connection *dns_add_query(s_header *h)
{

	s_connection * s = new s_connection;
	int id = rand() % 65536;

	/* set header flags */
	h->id[0] = s->id[0] = id >> 8; /* verified by dns_getresult_s() */
	h->id[1] = s->id[1] = id & 0xFF;
	h->flags1 = 0 | FLAGS1_MASK_RD;
	h->flags2 = 0;
	h->qdcount = 1;
	h->ancount = 0;
	h->nscount = 0;
	h->arcount = 0;
	s->want_list = 0;
	s->fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (s->fd != -1)
	{
		if (fcntl(s->fd, F_SETFL, O_NONBLOCK) != 0)
		{
			shutdown(s->fd,2);
			close(s->fd);
			s->fd = -1;
		}
	}
	if (s->fd != -1)
	{
		sockaddr_in addr;
		memset(&addr,0,sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = 0;
		addr.sin_addr.s_addr = INADDR_ANY;
		if (bind(s->fd,(sockaddr *)&addr,sizeof(addr)) != 0)
		{
			shutdown(s->fd,2);
			close(s->fd);
			s->fd = -1;
		}
	}
	if (s->fd == -1)
	{
		DELETE(s);
		return NULL;
	}
	/* create new connection object, add to linked list */
#ifdef THREADED_DNS
	pthread_mutex_lock(&connmap_lock);
#endif
	if (connections.find(s->fd) == connections.end())
		connections[s->fd] = s;
#ifdef THREADED_DNS
	pthread_mutex_unlock(&connmap_lock);
#endif

	return s;
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

in_addr* DNS::dns_aton4(const char * const ipstring)
{
	static in_addr ip;
	return dns_aton4_s(ipstring,&ip);
}

in_addr* DNS::dns_aton4_r(const char *ipstring) { /* ascii to numeric (reentrant): convert string to new 4part IP addr struct */
	in_addr* ip;
	ip = new in_addr;
	if(dns_aton4_s(ipstring,ip) == NULL)
	{
		DELETE(ip);
		return NULL;
	}
	return ip;
}

in_addr* DNS::dns_aton4_s(const char *ipstring, in_addr *ip) { /* ascii to numeric (buffered): convert string to given 4part IP addr struct */
	inet_aton(ipstring,ip);
	return ip;
}

int DNS::dns_getip4(const char *name) { /* build, add and send A query; retrieve result with dns_getresult() */
	s_header h;
	s_connection *s;
	int l;

	dns_init();
	

	l = dns_build_query_payload(name,DNS_QRY_A,1,(unsigned char *)&h.payload);
	if (l == -1)
		return -1;
	s = dns_add_query(&h);
	if (s == NULL)
		return -1;
	s->_class = 1;
	s->type = DNS_QRY_A;
	if (dns_send_requests(&h,s,l) == -1)
		return -1;

	return s->fd;
}

int DNS::dns_getip4list(const char *name) { /* build, add and send A query; retrieve result with dns_getresult() */
	s_header h;
	s_connection *s;
	int l;

	dns_init();
	
	l = dns_build_query_payload(name,DNS_QRY_A,1,(unsigned char *)&h.payload);
	if (l == -1)
		return -1;
	s = dns_add_query(&h);
	if (s == NULL)
		return -1;
	s->_class = 1;
	s->type = DNS_QRY_A;
	s->want_list = 1;
	if (dns_send_requests(&h,s,l) == -1)
		return -1;

	return s->fd;
}

int DNS::dns_getname4(const in_addr *ip) { /* build, add and send PTR query; retrieve result with dns_getresult() */
	char query[512];
	s_header h;
	s_connection * s;
	unsigned char *c;
	int l;

	c = (unsigned char *)&ip->s_addr;

	sprintf(query,"%d.%d.%d.%d.in-addr.arpa",c[3],c[2],c[1],c[0]);

	l = dns_build_query_payload(query,DNS_QRY_PTR,1,(unsigned char *)&h.payload);
	if (l == -1)
		return -1;
	s = dns_add_query(&h);
	if (s == NULL)
		return -1;
	s->_class = 1;
	s->type = DNS_QRY_PTR;
	if (dns_send_requests(&h,s,l) == -1)
		return -1;

	return s->fd;
}

char* DNS::dns_ntoa4(const in_addr * const ip) { /* numeric to ascii: convert 4part IP addr struct to static string */
	static char r[256];
	return dns_ntoa4_s(ip,r);
}

char* DNS::dns_ntoa4_s(const in_addr *ip, char *r) { /* numeric to ascii (buffered): convert 4part IP addr struct to given string */
	unsigned char *m;
	m = (unsigned char *)&ip->s_addr;
	sprintf(r,"%d.%d.%d.%d",m[0],m[1],m[2],m[3]);
	return r;
}

char* DNS::dns_getresult(const int cfd) { /* retrieve result of DNS query */
	log(DEBUG,"DNS: dns_getresult with cfd=%d",cfd);
	return dns_getresult_s(cfd,this->localbuf);
}

char* DNS::dns_getresult_s(const int cfd, char *res) { /* retrieve result of DNS query (buffered) */
	s_header h;
	s_connection *c;
	int l, i, q, curanswer, o;
	s_rr_middle rr;
	unsigned char buffer[sizeof(s_header)];
	unsigned short p;

	if (res)
		*res = 0;

	/* FireDNS used a linked list for this. How ugly (and slow). */

#ifdef THREADED_DNS
	/* XXX: STL really does NOT like being poked and prodded in more than
	 * one orifice by threaded apps. Make sure we remain nice to it, and
	 * lock a mutex around any access to the std::map.
	 */
	pthread_mutex_lock(&connmap_lock);
#endif
	connlist_iter n_iter = connections.find(cfd);
	if (n_iter == connections.end())
	{
		log(DEBUG,"DNS: got a response for a query we didnt send with fd=%d",cfd);
		return NULL;
	}
	else
	{
		/* Remove the query from the list */
		c = (s_connection*)n_iter->second;
		/* We don't delete c here, because its done later when needed */
		connections.erase(n_iter);
	}
#ifdef THREADED_DNS
	pthread_mutex_unlock(&connmap_lock);
#endif

	l = recv(c->fd,buffer,sizeof(s_header),0);
	dns_close(c->fd);
	if (l < 12)
	{
		DELETE(c);
		return NULL;
	}
	dns_fill_header(&h,buffer,l - 12);
	if (c->id[0] != h.id[0] || c->id[1] != h.id[1])
	{
		log(DEBUG,"DNS: id mismatch on query");
		DELETE(c);
		return NULL; /* ID mismatch */
	}
	if ((h.flags1 & FLAGS1_MASK_QR) == 0)
	{
		log(DEBUG,"DNS: didnt get a query result");
		DELETE(c);
		return NULL;
	}
	if ((h.flags1 & FLAGS1_MASK_OPCODE) != 0)
	{
		log(DEBUG,"DNS: got an OPCODE and didnt want one");
		DELETE(c);
		return NULL;
	}
	if ((h.flags2 & FLAGS2_MASK_RCODE) != 0)
	{
		log(DEBUG,"DNS lookup failed due to SERVFAIL");
		DELETE(c);
		return NULL;
	}
	if (h.ancount < 1)
	{
		log(DEBUG,"DNS: no answers!");
		DELETE(c);
		return NULL;
	}
	i = 0;
	q = 0;
	l -= 12;
	while ((unsigned)q < h.qdcount && i < l)
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
		while (q == 0 && i < l)
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
		if (l - i < 10)
		{
			DELETE(c);
			return NULL;
		}
		dns_fill_rr(&rr,&h.payload[i]);
		i += 10;
		if (rr.type != c->type)
		{
			curanswer++;
			i += rr.rdlength;
			continue;
		}
		if (rr._class != c->_class)
		{
			curanswer++;
			i += rr.rdlength;
			continue;
		}
		break;
	}
	if ((unsigned)curanswer == h.ancount)
		return NULL;
	if ((unsigned)i + rr.rdlength > (unsigned)l)
		return NULL;
	if (rr.rdlength > 1023)
		return NULL;

	switch (rr.type)
	{
		case DNS_QRY_PTR:
			log(DEBUG,"DNS: got a result of type DNS_QRY_PTR");
			o = 0;
			q = 0;
			while (q == 0 && i < l && o + 256 < 1023)
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
			if (c->want_list)
			{
				dns_ip4list *alist = (dns_ip4list *) res; /* we have to trust that this is aligned */
				while ((char *)alist - (char *)res < 700)
				{
					if (rr.type != DNS_QRY_A)
						break;
					if (rr._class != 1)
						break;
					if (rr.rdlength != 4)
					{
						DELETE(c);
						return NULL;
					}
					memcpy(&alist->ip,&h.payload[i],4);
					if ((unsigned)++curanswer >= h.ancount)
						break;
					i += rr.rdlength;
					q = 0;
					while (q == 0 && i < l)
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
							else i += h.payload[i] + 1;
						}
					}
					if (l - i < 10)
					{
						DELETE(c);
						return NULL;
					}
					dns_fill_rr(&rr,&h.payload[i]);
					i += 10;
					alist->next = (dns_ip4list *) dns_align(((char *) alist) + sizeof(dns_ip4list));
					alist = alist->next;
					alist->next = NULL;
				}
				alist->next = NULL;
				break;
			}
			memcpy(res,&h.payload[i],rr.rdlength);
			res[rr.rdlength] = '\0';
			break;
		default:
			memcpy(res,&h.payload[i],rr.rdlength);
			res[rr.rdlength] = '\0';
			break;
	}
	DELETE(c);
	return res;
}

DNS::DNS()
{
	dns_init();
	log(DEBUG,"Create blank DNS");
}

DNS::DNS(const std::string &dnsserver)
{
	dns_init_2(dnsserver.c_str());
	log(DEBUG,"Create DNS with server '%s'",dnsserver.c_str());
}

void DNS::SetNS(const std::string &dnsserver)
{
	dns_init_2(dnsserver.c_str());
	log(DEBUG,"Set NS");
}

DNS::~DNS()
{
}

bool DNS::ReverseLookup(const std::string &ip, bool ins)
{
	if (ServerInstance && ServerInstance->stats)
		ServerInstance->stats->statsDns++;
	binip = dns_aton4(ip.c_str());
	if (binip == NULL)
	{
		return false;
	}

	this->myfd = dns_getname4(binip);
	if (this->myfd == -1)
	{
		return false;
	}
	log(DEBUG,"DNS: ReverseLookup, fd=%d",this->myfd);
#ifndef THREADED_DNS
	if (ins)
	{
		if (ServerInstance && ServerInstance->SE)
			ServerInstance->SE->AddFd(this->myfd,true,X_ESTAB_DNS);
	}
#endif
	return true;
}

bool DNS::ForwardLookup(const std::string &host, bool ins)
{
	if (ServerInstance && ServerInstance->stats)
		ServerInstance->stats->statsDns++;
	this->myfd = dns_getip4(host.c_str());
	if (this->myfd == -1)
	{
		return false;
	}
	log(DEBUG,"DNS: ForwardLookup, fd=%d",this->myfd);
#ifndef THREADED_DNS
	if (ins)
	{
		if (ServerInstance && ServerInstance->SE)
			ServerInstance->SE->AddFd(this->myfd,true,X_ESTAB_DNS);
	}
#endif
	return true;
}

bool DNS::ForwardLookupWithFD(const std::string &host, int &fd)
{
	if (ServerInstance && ServerInstance->stats)
		ServerInstance->stats->statsDns++;
	this->myfd = dns_getip4(host.c_str());
	fd = this->myfd;
	if (this->myfd == -1)
	{
		return false;
	}
	log(DEBUG,"DNS: ForwardLookupWithFD, fd=%d",this->myfd);
	if (ServerInstance && ServerInstance->SE)
		ServerInstance->SE->AddFd(this->myfd,true,X_ESTAB_MODULE);
	return true;
}

bool DNS::HasResult(int fd)
{
	return (fd == this->myfd);
}

/* Only the multithreaded dns uses this poll() based
 * check now. As its in another thread we dont have
 * to worry about its performance that much.
 */
bool DNS::HasResult()
{
	log(DEBUG,"DNS: HasResult, fd=%d",this->myfd);
	pollfd polls;
	polls.fd = this->myfd;
	polls.events = POLLIN;
	int ret = poll(&polls,1,1);
	log(DEBUG,"DNS: Hasresult returning %d",ret);
	return (ret > 0);
}

int DNS::GetFD()
{
	return this->myfd;
}

std::string DNS::GetResult()
{
	log(DEBUG,"DNS: GetResult()");
	result = dns_getresult(this->myfd);
	if (result)
	{
		if (ServerInstance && ServerInstance->stats)
			ServerInstance->stats->statsDnsGood++;
		dns_close(this->myfd);
		this->myfd = -1;
		return result;
	}
	else
	{
		if (ServerInstance && ServerInstance->stats)
			ServerInstance->stats->statsDnsBad++;
		if (this->myfd != -1)
		{
			dns_close(this->myfd);
			this->myfd = -1;
		}
		return "";
	}
}

std::string DNS::GetResultIP()
{
	char r[1024];
	log(DEBUG,"DNS: GetResultIP()");
	result = dns_getresult(this->myfd);
	if (this->myfd != -1)
	{
		dns_close(this->myfd);
		this->myfd = -1;
	}
	if (result)
	{
		if (ServerInstance && ServerInstance->stats)
			ServerInstance->stats->statsDnsGood++;
		unsigned char a = (unsigned)result[0];
		unsigned char b = (unsigned)result[1];
		unsigned char c = (unsigned)result[2];
		unsigned char d = (unsigned)result[3];
		snprintf(r,1024,"%u.%u.%u.%u",a,b,c,d);
		return r;
	}
	else
	{
		if (ServerInstance && ServerInstance->stats)
			ServerInstance->stats->statsDnsBad++;
		log(DEBUG,"DANGER WILL ROBINSON! NXDOMAIN for forward lookup, but we got a reverse lookup!");
		return "";
	}
}



#ifdef THREADED_DNS

/* This function is a thread function which can be thought of as a lightweight process
 * to all you non-threaded people. In actuality its so much more, and pretty damn cool.
 * With threaded dns enabled, each user which connects gets a thread attached to their
 * user record when their DNS lookup starts. This function starts in parallel, and
 * commences a blocking dns lookup. Because its a seperate thread, this occurs without
 * actually blocking the main application. Once the dns lookup is completed, the thread
 * checks if the user is still around by checking their fd against the reference table,
 * and if they are, writes the hostname into the struct and terminates, after setting
 * userrec::dns_done to true. Because this is multi-threaded it can make proper use of
 * SMP setups (like the one i have here *grin*).
 * This is in comparison to the non-threaded dns, which must monitor the thread sockets
 * in a nonblocking fashion, consuming more resources to do so.
 */
void* dns_task(void* arg)
{
	userrec* u = (userrec*)arg;
	int thisfd = u->fd;

	log(DEBUG,"DNS thread for user %s",u->nick);
	DNS dns1;
	DNS dns2;
	std::string host;
	std::string ip;
	if (dns1.ReverseLookup(inet_ntoa(u->ip4),false))
	{
		while (!dns1.HasResult())
			usleep(100);
		host = dns1.GetResult();
		if (host != "")
		{
			if (dns2.ForwardLookup(host, false))
			{
				while (!dns2.HasResult())
					usleep(100);
				ip = dns2.GetResultIP();
				if (ip == std::string(inet_ntoa(u->ip4)))
				{
					if (host.length() < 65)
					{
						if ((fd_ref_table[thisfd] == u) && (fd_ref_table[thisfd]))
						{
							if (!u->dns_done)
							{
								strcpy(u->host,host.c_str());
								if ((fd_ref_table[thisfd] == u) && (fd_ref_table[thisfd]))
								{
									strcpy(u->dhost,host.c_str());
								}
							}
						}
					}
				}
			}
		}
	}
	if ((fd_ref_table[thisfd] == u) && (fd_ref_table[thisfd]))
		u->dns_done = true;
	pthread_exit(0);
}
#endif

Resolver::Resolver(const std::string &source, bool forward, const std::string &dnsserver = "") : input(source), fwd(forward), server(dnsserver)
{
	if (this->server != "")
		Query.SetNS(this->server);
	else
		Query.SetNS(Config->DNSServer);

	if (forward)
	{
		Query.ForwardLookup(input.c_str(), false);
		this->fd = Query.GetFD();
	}
	else
	{
		Query.ReverseLookup(input.c_str(), false);
		this->fd = Query.GetFD();
	}
	if (fd < 0)
	{
		log(DEBUG,"Resolver::Resolver: RESOLVER_NSDOWN");
		this->OnError(RESOLVER_NSDOWN);
		ModuleException e("Resolver: Nameserver is down");
		throw e;
		/* We shouldnt get here really */
		return;
	}

	if (ServerInstance && ServerInstance->SE)
	{
		log(DEBUG,"Resolver::Resolver: this->fd=%d",this->fd);
		ServerInstance->SE->AddFd(this->fd,true,X_ESTAB_CLASSDNS);
	}
	else
	{
		log(DEBUG,"Resolver::Resolver: RESOLVER_NOTREADY");
		this->OnError(RESOLVER_NOTREADY);
		ModuleException e("Resolver: Core not initialized yet");
		throw e;
		/* We shouldnt get here really */
		return;
	}
}

Resolver::~Resolver()
{
	log(DEBUG,"Resolver::~Resolver");
	if (ServerInstance && ServerInstance->SE)
		ServerInstance->SE->DelFd(this->fd);
}

int Resolver::GetFd()
{
	return this->fd;
}

bool Resolver::ProcessResult()
{
	log(DEBUG,"Resolver::ProcessResult");
	if (this->fwd)
		result = Query.GetResultIP();
	else
		result = Query.GetResult();

	if (result != "")
	{
		log(DEBUG,"Resolver::OnLookupComplete(%s)",result.c_str());
		this->OnLookupComplete(result);
		return true;
	}
	else
	{
		log(DEBUG,"Resolver::OnError(RESOLVER_NXDOMAIN)");
		this->OnError(RESOLVER_NXDOMAIN);
		return false;
	}
}

void Resolver::OnLookupComplete(const std::string &result)
{
}

void Resolver::OnError(ResolverError e)
{
}

void dns_deal_with_classes(int fd)
{
	log(DEBUG,"dns_deal_with_classes(%d)",fd);
	if ((fd > -1) && (dns_classes[fd]))
	{
		log(DEBUG,"Valid fd %d",fd);
		dns_classes[fd]->ProcessResult();
		delete dns_classes[fd];
		dns_classes[fd] = NULL;
	}
}

bool dns_add_class(Resolver* r)
{
	log(DEBUG,"dns_add_class");
	if ((r) && (r->GetFd() > -1))
	{
		if (!dns_classes[r->GetFd()])
		{
			log(DEBUG,"dns_add_class: added class");
			dns_classes[r->GetFd()] = r;
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
	memset(dns_classes,0,sizeof(dns_classes));
}

