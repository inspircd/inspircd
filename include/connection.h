/*

*/

#include "inspircd_config.h"
#include "base.h"
#include <string>
#include <map>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <vector>

#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#define STATE_DISCONNECTED	0
#define STATE_CONNECTED		1
#define STATE_SYNC		2
#define STATE_NOAUTH_INBOUND	3
#define STATE_NOAUTH_OUTBOUND	4

/** Each connection has one or more of these
 * each represents ONE outbound connection to another ircd
 * so each inbound has multiple outbounds.
 */
class ircd_connector : public classbase
{
 private:
	/** Sockaddr of the outbound ip and port
	 */
	sockaddr_in addr;
	
	/** File descriptor of the outbound connection
	 */
	int fd;
	
	/** Server name
	 */
	std::string servername;
	
	/** Server names of servers that this server is linked to
	 * So for A->B->C, if this was the record for B it would contain A and C
	 * whilever both servers are connected to B.
	 */
	std::vector<std::string> routes;
	
	/** State. STATE_NOAUTH_INBOUND, STATE_NOAUTH_OUTBOUND
	 * STATE_SYNC, STATE_DISCONNECTED, STATE_CONNECTED
	 */
	int state;
	
	bool SetHostAddress(char* host, int port);

 public:
 
	bool MakeOutboundConnection(char* host, int port);
	std::string GetServerName();
	void SetServerName(std::string serv);
	int GetDescriptor();
	void SetDescriptor(int fd);
	int GetState();
	void SetState(int state);
};


class packet : public classbase
{
 public:
 	long key;
 	int id;
	short int type;
	char data[MAXBUF];

	packet();
	~packet();
};


class connection : public classbase
{
 public:
 	long key;
	int fd;			// file descriptor
	char host[256];		// hostname
	long ip;		// ipv4 address
	char inbuf[MAXBUF];	// recvQ
	long bytes_in;
	long bytes_out;
	long cmds_in;
	long cmds_out;
	bool haspassed;
	int port;
	int registered;
	short int state;
	time_t lastping;
	time_t signon;
	time_t idle_lastmsg;
	time_t nping;
	char internal_addr[1024];
	int internal_port;
	std::vector<ircd_connector> connectors;
	
	connection();
	bool CreateListener(char* host, int p);
	bool BeginLink(char* targethost, int port, char* password, char* servername);
	void TerminateLink(char* targethost);
	bool SendPacket(char *message, char* host);
	bool RecvPacket(char *message, char* host);
	ircd_connector* FindHost(std::string host);
	bool AddIncoming(int fd,char* targethost);
	long GenKey();
};


#endif

