#ifndef __MESSAGE_H
#define __MESSAGE_H

// include the common header files

#include <typeinfo>
#include <iostream>
#include <string>
#include <deque>
#include <sstream>
#include <vector>
#include "users.h"
#include "channels.h"

int common_channels(userrec *u, userrec *u2);
void chop(char* str);
void tidystring(char* str);
void safedelete(chanrec *p);
void safedelete(userrec *p);
void Blocking(int s);
void NonBlocking(int s);
int CleanAndResolve (char *resolvedHost, const char *unresolvedHost);
int c_count(userrec* u);
bool hasumode(userrec* user, char mode);
void ChangeName(userrec* user, const char* gecos);
void ChangeDisplayedHost(userrec* user, const char* host);
int isident(const char* n);
int isnick(const char* n);
char* cmode(userrec *user, chanrec *chan);
int cstatus(userrec *user, chanrec *chan);
int has_channel(userrec *u, chanrec *c);

#endif
