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
int common_channels(userrec *u, userrec *u2);
void WriteCommon(userrec *u, char* text, ...);
void WriteCommonExcept(userrec *u, char* text, ...);
void WriteWallOps(userrec *source, char* text, ...);
int isnick(const char *n);
userrec* Find(std::string nick);
chanrec* FindChan(const char* chan);
char* cmode(userrec *user, chanrec *chan);
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

