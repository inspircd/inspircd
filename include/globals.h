/*


*/


#ifndef __WORLD_H
#define __WORLD_H

// include the common header files

#include <typeinfo>
#include <iostream>
#include <string>
#include <deque>
#include "users.h"
#include "channels.h"

typedef std::deque<std::string> file_cache;

void WriteOpers(char* text, ...);
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
void WriteWallOps(userrec *source, bool local_only, char* text, ...);
int isnick(const char *n);
userrec* Find(std::string nick);
chanrec* FindChan(const char* chan);
char* cmode(userrec *user, chanrec *chan);
std::string getservername();
std::string getnetworkname();
std::string getadminname();
std::string getadminemail();
std::string getadminnick();
void readfile(file_cache &F, const char* fname);
int ModeDefiend(char c, int i);

#endif
