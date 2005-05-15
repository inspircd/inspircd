/*
dns.cpp - based on the firedns library Copyright (C) 2002 Ian Gulliver

This program is free software; you can redistribute it and/or modify
it under the terms of version 2 of the GNU General Public License as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#define _DNS_C

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
#include "dns.h"
#include "helperfuncs.h"

extern int statsAccept,statsRefused,statsUnknown,statsCollisions,statsDns,statsDnsGood,statsDnsBad,statsConnects,statsSent,statsRecv;

#define max(a,b) (a > b ? a : b)
#define DNS_MAX              8                    /* max number of nameservers used */
#define DNS_CONFIG_FBCK     "/etc/resolv.conf"    /* fallback config file */
#define DNS_PORT            53                    /* DNS well known port */
#define DNS_QRY_A            1                    /* name to IP address */
#define DNS_QRY_AAAA        28                    /* name to IP6 address */
#define DNS_QRY_PTR         12                    /* IP address to name */
#define DNS_QRY_MX          15                    /* name to MX */
#define DNS_QRY_TXT         16                    /* name to TXT */
#define DNS_QRY_CNAME       5

#define DNS_ALIGN (sizeof(void *) > sizeof(long) ? sizeof(void *) : sizeof(long))
#define DNS_TRIES 3
#define RESULTSIZE 1024
#define min(a,b) (a < b ? a : b)

static struct in_addr servers4[DNS_MAX]; /* up to DNS_MAX nameservers; populated by dns_init() */
static int i4; /* actual count of nameservers; set by dns_init() */

static int initdone = 0; /* to ensure dns_init() only runs once (on the first call) */
static int wantclose = 0;
static int lastcreate = -1;

struct s_connection { /* open DNS query */
	struct s_connection *next; /* next in list */
	unsigned char id[2];
	unsigned int _class;
	unsigned int type;
	int want_list;
	int fd; /* file descriptor returned from sockets */
};

struct s_rr_middle {
	unsigned int type;
	unsigned int _class;
	unsigned long ttl;
	unsigned int rdlength;
};

#define DNS_POINTER_VALUE 0xc000

static s_connection *connection_head = NULL; /* linked list of open DNS queries; populated by dns_add_query(), decimated by dns_getresult_s() */

struct s_header { /* DNS query header */
	unsigned char id[2];
	unsigned int flags1;
#define FLAGS1_MASK_QR 0x80
#define FLAGS1_MASK_OPCODE 0x78 /* bitshift right 3 */
#define FLAGS1_MASK_AA 0x04
#define FLAGS1_MASK_TC 0x02
#define FLAGS1_MASK_RD 0x01
	unsigned int flags2;
#define FLAGS2_MASK_RA 0x80
#define FLAGS2_MASK_Z  0x70
#define FLAGS2_MASK_RCODE 0x0f
	unsigned int qdcount;
	unsigned int ancount;
	unsigned int nscount;
	unsigned int arcount;
	unsigned char payload[512]; /* DNS question, populated by dns_build_query_payload() */
};

extern time_t TIME;

void *dns_align(void *inp) {
	char *p = (char*)inp;
	int offby = ((char *)p - (char *)0) % DNS_ALIGN;
	if (offby != 0)
		return p + (DNS_ALIGN - offby);
	else
		return p;
}

/*
 * These little hacks are here to avoid alignment and type sizing issues completely by doing manual copies
 */
void dns_fill_rr(s_rr_middle* rr, const unsigned char *input) {
	rr->type = input[0] * 256 + input[1];
	rr->_class = input[2] * 256 + input[3];
	rr->ttl = input[4] * 16777216 + input[5] * 65536 + input[6] * 256 + input[7];
	rr->rdlength = input[8] * 256 + input[9];
}

void dns_fill_header(s_header *header, const unsigned char *input, const int l) {
	header->id[0] = input[0];
	header->id[1] = input[1];
	header->flags1 = input[2];
	header->flags2 = input[3];
	header->qdcount = input[4] * 256 + input[5];
	header->ancount = input[6] * 256 + input[7];
	header->nscount = input[8] * 256 + input[9];
	header->arcount = input[10] * 256 + input[11];
	memcpy(header->payload,&input[12],l);
}

void dns_empty_header(unsigned char *output, const s_header *header, const int l) {
	output[0] = header->id[0];
	output[1] = header->id[1];
	output[2] = header->flags1;
	output[3] = header->flags2;
	output[4] = header->qdcount / 256;
	output[5] = header->qdcount % 256;
	output[6] = header->ancount / 256;
	output[7] = header->ancount % 256;
	output[8] = header->nscount / 256;
	output[9] = header->nscount % 256;
	output[10] = header->arcount / 256;
	output[11] = header->arcount % 256;
	memcpy(&output[12],header->payload,l);
}

void dns_close(int fd) { /* close query */
	if (fd == lastcreate) {
		wantclose = 1;
		return;
	}
	close(fd);
	return;
}

void DNS::dns_init() { /* on first call only: populates servers4 struct with up to DNS_MAX nameserver IP addresses from /etc/resolv.conf */
	FILE *f;
	int i;
	in_addr addr4;
	char buf[1024];
	if (initdone == 1)
		return;
	i4 = 0;

	initdone = 1;
	srand((unsigned int) TIME);
	memset(servers4,'\0',sizeof(in_addr) * DNS_MAX);
	f = fopen(DNS_CONFIG_FBCK,"r");
	if (f == NULL)
		return;
	while (fgets(buf,1024,f) != NULL) {
		if (strncmp(buf,"nameserver",10) == 0) {
			i = 10;
			while (buf[i] == ' ' || buf[i] == '\t')
				i++;
			if (i4 < DNS_MAX) {
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
        memset(servers4,'\0',sizeof(in_addr) * DNS_MAX);
        if (dns_aton4_s(dnsserver,&addr4) != NULL)
            memcpy(&servers4[i4++],&addr4,sizeof(in_addr));
}


static int dns_send_requests(const s_header *h, const s_connection *s, const int l)
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
	addr4.sin_port = htons(DNS_PORT);
	if (sendto(s->fd, payload, l + 12, 0, (sockaddr *) &addr4, sizeof(addr4)) == -1)
	{
		return -1;
	}

	return 0;
}

static s_connection *dns_add_query(s_header *h) { /* build DNS query, add to list */
	s_connection * s;

	s = new s_connection;

	/* set header flags */
	h->id[0] = s->id[0] = rand() % 255; /* verified by dns_getresult_s() */
	h->id[1] = s->id[1] = rand() % 255;
	h->flags1 = 0 | FLAGS1_MASK_RD;
	h->flags2 = 0;
	h->qdcount = 1;
	h->ancount = 0;
	h->nscount = 0;
	h->arcount = 0;

	/* turn off want_list by default */
	s->want_list = 0;

	/* try to create ipv6 or ipv4 socket */
		s->fd = socket(PF_INET, SOCK_DGRAM, 0);
		if (s->fd != -1) {
			if (fcntl(s->fd, F_SETFL, O_NONBLOCK) != 0) {
				close(s->fd);
				s->fd = -1;
			}
		}
		if (s->fd != -1) {
			sockaddr_in addr;
			memset(&addr,0,sizeof(addr));
			addr.sin_family = AF_INET;
			addr.sin_port = 0;
			addr.sin_addr.s_addr = INADDR_ANY;
			if (bind(s->fd,(sockaddr *)&addr,sizeof(addr)) != 0) {
				close(s->fd);
				s->fd = -1;
			}
		}
		if (s->fd == -1) {
			delete s;
			return NULL;
		}
	/* create new connection object, add to linked list */
	s->next = connection_head;
	connection_head = s;

	if (wantclose == 1) {
		close(lastcreate);
		wantclose = 0;
	}
	lastcreate = s->fd;
	return s;
}

static int dns_build_query_payload(const char * const name, const unsigned short rr, const unsigned short _class, unsigned char * const payload) { 
	short payloadpos;
	const char * tempchr, * tempchr2;
	unsigned short l;
	
	payloadpos = 0;
	tempchr2 = name;

	/* split name up into labels, create query */
	while ((tempchr = strchr(tempchr2,'.')) != NULL) {
		l = tempchr - tempchr2;
		if (payloadpos + l + 1 > 507)
			return -1;
		payload[payloadpos++] = l;
		memcpy(&payload[payloadpos],tempchr2,l);
		payloadpos += l;
		tempchr2 = &tempchr[1];
	}
	l = strlen(tempchr2);
	if (l) {
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

in_addr* DNS::dns_aton4(const char * const ipstring) { /* ascii to numeric: convert string to static 4part IP addr struct */
	static in_addr ip;
	return dns_aton4_s(ipstring,&ip);
}

in_addr* DNS::dns_aton4_r(const char *ipstring) { /* ascii to numeric (reentrant): convert string to new 4part IP addr struct */
	in_addr* ip;
	ip = new in_addr;
	if(dns_aton4_s(ipstring,ip) == NULL) {
		delete ip;
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

char* DNS::dns_ntoa4_r(const in_addr *ip) { /* numeric to ascii (reentrant): convert 4part IP addr struct to new string */
	char *r;
	r = new char[256];
	return dns_ntoa4_s(ip,r);
}

char* DNS::dns_ntoa4_s(const in_addr *ip, char *r) { /* numeric to ascii (buffered): convert 4part IP addr struct to given string */
	unsigned char *m;
	m = (unsigned char *)&ip->s_addr;
	sprintf(r,"%d.%d.%d.%d",m[0],m[1],m[2],m[3]);
	return r;
}

char* DNS::dns_getresult(const int cfd) { /* retrieve result of DNS query */
	static char r[RESULTSIZE];
	return dns_getresult_s(cfd,r);
}

char* DNS::dns_getresult_r(const int cfd) { /* retrieve result of DNS query (reentrant) */
	char *r;
	r = new char[RESULTSIZE];
	if(dns_getresult_s(cfd,r) == NULL) {
		delete r;
		return NULL;
	}
	return r;
}

char* DNS::dns_getresult_s(const int cfd, char *res) { /* retrieve result of DNS query (buffered) */
	s_header h;
	s_connection *c, *prev;
	int l,i,q,curanswer,o;
	s_rr_middle rr;
	unsigned char buffer[sizeof(s_header)];
	unsigned short p;

	if (res)
	{
		res[0] = 0;
	}

	prev = NULL;
	c = connection_head;
	while (c != NULL) { /* find query in list of open queries */
		if (c->fd == cfd)
			break;
		prev = c;
		c = c->next;
	}
	if (c == NULL) {
		return NULL; /* query not found */
	}
	/* query found-- pull from list: */
	if (prev != NULL)
		prev->next = c->next;
	else
		connection_head = c->next;

	l = recv(c->fd,buffer,sizeof(s_header),0);
	dns_close(c->fd);
	if (l < 12) {
		delete c;
		return NULL;
	}
	dns_fill_header(&h,buffer,l - 12);
	if (c->id[0] != h.id[0] || c->id[1] != h.id[1]) {
		delete c;
		return NULL; /* ID mismatch */
	}
	if ((h.flags1 & FLAGS1_MASK_QR) == 0) {
		delete c;
		return NULL;
	}
	if ((h.flags1 & FLAGS1_MASK_OPCODE) != 0) {
		delete c;
		return NULL;
	}
	if ((h.flags2 & FLAGS2_MASK_RCODE) != 0) {
		delete c;
		return NULL;
	}
	if (h.ancount < 1)  { /* no sense going on if we don't have any answers */
		delete c;
		return NULL;
	}
	/* skip queries */
	i = 0;
	q = 0;
	l -= 12;
	while (q < h.qdcount && i < l) {
		if (h.payload[i] > 63) { /* pointer */
			i += 6; /* skip pointer, _class and type */
			q++;
		} else { /* label */
			if (h.payload[i] == 0) {
				q++;
				i += 5; /* skip nil, _class and type */
			} else
				i += h.payload[i] + 1; /* skip length and label */
		}
	}
	/* &h.payload[i] should now be the start of the first response */
	curanswer = 0;
	while (curanswer < h.ancount) {
		q = 0;
		while (q == 0 && i < l) {
			if (h.payload[i] > 63) { /* pointer */
				i += 2; /* skip pointer */
				q = 1;
			} else { /* label */
				if (h.payload[i] == 0) {
					i++;
					q = 1;
				} else
					i += h.payload[i] + 1; /* skip length and label */
			}
		}
		if (l - i < 10) {
			delete c;
			return NULL;
		}
		dns_fill_rr(&rr,&h.payload[i]);
		i += 10;
		if (rr.type != c->type) {
			curanswer++;
			i += rr.rdlength;
			continue;
		}
		if (rr._class != c->_class) {
			curanswer++;
			i += rr.rdlength;
			continue;
		}
		break;
	}
	if (curanswer == h.ancount)
		return NULL;
	if (i + rr.rdlength > l)
		return NULL;
	if (rr.rdlength > 1023)
		return NULL;

	switch (rr.type) {
		case DNS_QRY_PTR:
			o = 0;
			q = 0;
			while (q == 0 && i < l && o + 256 < 1023) {
				if (h.payload[i] > 63) { /* pointer */
					memcpy(&p,&h.payload[i],2);
					i = ntohs(p) - DNS_POINTER_VALUE - 12;
				} else { /* label */
					if (h.payload[i] == 0)
						q = 1;
					else {
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
			if (c->want_list) {
				dns_ip4list *alist = (dns_ip4list *) res; /* we have to trust that this is aligned */
				while ((char *)alist - (char *)res < 700) {
					if (rr.type != DNS_QRY_A)
						break;
					if (rr._class != 1)
						break;
					if (rr.rdlength != 4) {
						delete c;
						return NULL;
					}
					memcpy(&alist->ip,&h.payload[i],4);
					if (++curanswer >= h.ancount)
						break;
					i += rr.rdlength;
					{
						/* skip next name */
						q = 0;
						while (q == 0 && i < l) {
							if (h.payload[i] > 63) { /* pointer */
								i += 2; /* skip pointer */
								q = 1;
							} else { /* label */
								if (h.payload[i] == 0) {
									i++;
									q = 1;
								} else
									i += h.payload[i] + 1; /* skip length and label */
							}
						}
					}
					if (l - i < 10) {
						delete c;
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
			goto defaultcase;
			break;
		default:
		defaultcase:
			memcpy(res,&h.payload[i],rr.rdlength);
			res[rr.rdlength] = '\0';
			break;
	}
	delete c;
	return res;
}

DNS::DNS()
{
	dns_init();
}

DNS::DNS(std::string dnsserver)
{
	dns_init_2(dnsserver.c_str());
}

void DNS::SetNS(std::string dnsserver)
{
	dns_init_2(dnsserver.c_str());
}

DNS::~DNS()
{
}

bool DNS::ReverseLookup(std::string ip)
{
	statsDns++;
        binip = dns_aton4(ip.c_str());
        if (binip == NULL) {
                return false;
        }

        this->fd = dns_getname4(binip);
	if (this->fd == -1)
	{
		return false;
	}
	return true;
}

bool DNS::ForwardLookup(std::string host)
{
}

bool DNS::HasResult()
{
	pollfd polls;
	polls.fd = this->fd;
	polls.events = POLLIN;
	int ret = poll(&polls,1,1);
	return (ret > 0);
}

int DNS::GetFD()
{
	return this->fd;
}

std::string DNS::GetResult()
{
        result = dns_getresult(this->fd);
        if (result) {
		statsDnsGood++;
		dns_close(this->fd);
		return result;
        } else {
		statsDnsBad++;
		return "";
	}
}
