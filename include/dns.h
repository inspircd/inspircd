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


/** The DNS class allows fast nonblocking resolution of hostnames
 * and ip addresses. It is based heavily upon firedns by Ian Gulliver.
 */
class DNS
{
private:
	in_addr *binip;
	char* result;
	char localbuf[1024];
	int t;
	void dns_init();
	int myfd;
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
	/** The default constructor uses dns addresses read from /etc/resolv.conf.
	 * Please note that it will re-read /etc/resolv.conf for each copy of the
	 * class you instantiate, causing disk access and slow lookups if you create
	 * a lot of them. Consider passing the constructor a server address as a parameter
	 * instead.
	 */
	DNS();
	/** This constructor accepts a dns server address. The address must be in dotted
	 * decimal form, e.g. 1.2.3.4.
	 */
	DNS(const std::string &dnsserver);
	/** The destructor frees all used structures.
	 */
	~DNS();
	/** This method will start the reverse lookup of an ip given in dotted decimal
	 * format, e.g. 1.2.3.4, and returns true if the lookup was successfully
	 * initiated.
	 */
	bool ReverseLookup(const std::string &ip);
	/** This method will start the forward lookup of a hostname, e.g. www.inspircd.org,
	 * and returns true if the lookup was successfully initiated.
	 */
	bool ForwardLookup(const std::string &host);
	/** Used by modules to perform a dns lookup but have the socket engine poll a module, instead of the dns object directly.
	 */
	bool ForwardLookupWithFD(const std::string &host, int &fd);
	/** This method will return true when the lookup is completed. It uses poll internally
	 * to determine the status of the socket.
	 */
	bool HasResult();
	/** This method will return true if the lookup's fd matches the one provided
	 */
	bool HasResult(int fd);
	/** This method returns the result of your query as a string, depending upon wether you
	 * called DNS::ReverseLookup() or DNS::ForwardLookup.
	 */
	std::string GetResult();
	std::string GetResultIP();
	/** This method returns the file handle used by the dns query socket or zero if the
	 * query is invalid for some reason, e.g. the dns server not responding.
	 */
	int GetFD();
	void SetNS(const std::string &dnsserver);
};

/** This is the handler function for multi-threaded DNS.
 * It cannot be a class member as pthread will not let us
 * create a thread whos handler function is a member of
 * a class (ugh).
 */
void* dns_task(void* arg);

void dns_close(int fd);

#endif
