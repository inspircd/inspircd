/*


*/


#include <string>
#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <netdb.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/param.h>
#include <sys/types.h>

#ifndef _LINUX_C_LIB_VERSION
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#endif

#include <arpa/inet.h>
#include <string>
#include <deque>

#include "inspircd_config.h"
#include "inspircd_io.h"
#include "inspircd_util.h"
#include "users.h"
#include "channels.h"

// some misc defines

#define ERROR -1
#define TRUE 1
#define FALSE 0
#define IDENTMAX 9
#define MAXSOCKS 64

// maximum lengths of items

#define MAXQUIT 255
#define MAXCOMMAND 32
#define MAXTOPIC 307
#define MAXKICK 255

// flags for use with log()

#define DEBUG 10
#define VERBOSE 20
#define DEFAULT 30
#define SPARSE 40
#define NONE 50

// flags for use with WriteMode

#define WM_AND 1
#define WM_OR 2

// flags for use with OnUserPreMessage and OnUserPreNotice

#define TYPE_USER 1
#define TYPE_CHANNEL 2

typedef std::deque<std::string> file_cache;

/* prototypes */
int InspIRCd(void);
int InitConfig(void);
void Error(int status);
void send_error(char *s);
void ReadConfig(void);
void strlower(char *n);

void WriteOpers(char* text, ...);
void WriteMode(const char* modes, int flags, const char* text, ...);
void log(int level, char *text, ...);
void Write(int sock,char *text, ...);
void WriteServ(int sock, char* text, ...);
void WriteFrom(int sock, userrec *user,char* text, ...);
void WriteTo(userrec *source, userrec *dest,char *data, ...);
void WriteChannel(chanrec* Ptr, userrec* user, char* text, ...);
void ChanExceptSender(chanrec* Ptr, userrec* user, char* text, ...);
void WriteCommon(userrec *u, char* text, ...);
void WriteCommonExcept(userrec *u, char* text, ...);
void WriteWallOps(userrec *source, bool local_only, char* text, ...);
userrec* Find(std::string nick);
chanrec* FindChan(const char* chan);
std::string getservername();
std::string getserverdesc();
std::string getnetworkname();
std::string getadminname();
std::string getadminemail();
std::string getadminnick();
void readfile(file_cache &F, const char* fname);
bool ModeDefined(char c, int i);
bool ModeDefinedOper(char c, int i);
int ModeDefinedOn(char c, int i);
int ModeDefinedOff(char c, int i);
chanrec* add_channel(userrec *user, const char* cn, const char* key, bool override);
chanrec* del_channel(userrec *user, const char* cname, const char* reason, bool local);
void force_nickchange(userrec* user,const char* newnick);
void kill_link(userrec *user,const char* r);
int usercount(chanrec *c);
void call_handler(const char* commandname,char **parameters, int pcnt, userrec *user);
void send_network_quit(const char* nick, const char* reason);
long GetRevision();

// mesh network functions

void NetSendToCommon(userrec* u, char* s);
void NetSendToAll(char* s);
void NetSendToAllAlive(char* s);
void NetSendToOne(char* target,char* s);
void NetSendToAllExcept(const char* target,char* s);
void NetSendMyRoutingTable();
void DoSplit(const char* params);
void RemoveServer(const char* name);


