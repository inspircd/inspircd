/*

$Log$
Revision 1.1  2003/01/23 19:45:58  brain
Initial revision

Revision 1.5  2003/01/22 20:49:16  brain
Added FileReader file-caching class
Changed m_randquote to use FileReader class

Revision 1.4  2003/01/15 22:47:44  brain
Changed user and channel structs to classes (finally)

Revision 1.3  2003/01/13 22:30:50  brain
Added Admin class (holds /admin info for modules)
Added methods to Server class


*/


#ifndef __WORLD_H
#define __WORLD_H

// include the common header files

#include <typeinfo>
#include <iostream.h>
#include <string>
#include <deque>
#include "users.h"
#include "channels.h"

typedef deque<string> file_cache;

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

#endif
