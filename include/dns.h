/*
dns.h - dns library declarations based on firedns Copyright (C) 2002 Ian Gulliver

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

#ifndef _DNS_H
#define _DNS_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>

struct dns_ip4list {
	in_addr ip;
	dns_ip4list *next;
};

class DNS
{
private:
	char *result;
	in_addr *binip;
	int t,i;
	void dns_init();
	unsigned int fd;
	void dns_init_2(const char* dnsserver);
	in_addr *dns_aton4(const char * const ipstring);
	char *dns_ntoa4(const in_addr * const ip);
	int dns_getip4(const char * const name);
	int dns_getip4list(const char * const name);
	int dns_getname4(const in_addr * const ip);
	char *dns_getresult(const int fd);
	in_addr *dns_aton4_s(const char * const ipstring, in_addr * const ip);
	char *dns_ntoa4_s(const in_addr * const ip, char * const result);
	char *dns_getresult_s(const int fd, char * const result);
	in_addr *dns_aton4_r(const char * const ipstring);
	char *dns_ntoa4_r(const in_addr * const ip);
	char *dns_getresult_r(const int fd);
public:
	DNS();
	DNS(std::string dnsserver);
	~DNS();
	bool ReverseLookup(std::string ip);
	bool ForwardLookup(std::string host);
	bool HasResult();
	std::string GetResult();
	int GetFD();
};

#endif
