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

#define ERROR -1
#define TRUE 1
#define FALSE 0
#define IDENTMAX 9
/* max sockets we can open */
#define MAXSOCKS 64

typedef deque<string> file_cache;

/* prototypes */
int InspIRCd(void);
int InitConfig(void);
void Error(int status);
void send_error(char *s);
void ReadConfig(void);
void strlower(char *n);

void WriteOpers(char* text, ...);
void debug(char *text, ...);
void Write(int sock,char *text, ...);
void WriteServ(int sock, char* text, ...);
void WriteFrom(int sock, userrec *user,char* text, ...);
void WriteTo(userrec *source, userrec *dest,char *data, ...);
void WriteChannel(chanrec* Ptr, userrec* user, char* text, ...);
void ChanExceptSender(chanrec* Ptr, userrec* user, char* text, ...);
int common_channels(userrec *u, userrec *u2);
void WriteCommon(userrec *u, char* text, ...);
void WriteCommonExcept(userrec *u, char* text, ...);
void WriteWallOps(userrec *source, char* text, ...);
int isnick(const char *n);
userrec* Find(string nick);
chanrec* FindChan(const char* chan);
char* cmode(userrec *user, chanrec *chan);
string getservername();
string getnetworkname();
string getadminname();
string getadminemail();
string getadminnick();
void readfile(file_cache &F, const char* fname);

