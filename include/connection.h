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

#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#define PT_SYN_ONLY 0
#define PT_ACK_ONLY 1
#define PT_SYN_WITH_DATA 2
#define PT_KEY_EXCHANGE 3


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
	
	connection();
	bool CreateListener(char* host, int p);
	bool BeginLink(char* targethost, int port, char* password);
	void TerminateLink(char* targethost);
	bool SendPacket(char *message, char* host, int port, long ourkey);
	bool RecvPacket(char *message, char* host, int &prt, long &theirkey);
	bool SendSYN(char* host, int port);
	bool SendACK(char* host, int port, int reply_id);
	long GenKey();
};


#endif

