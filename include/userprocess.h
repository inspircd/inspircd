#ifndef __USERPROCESS_H__
#define __USERPROCESS_H__

#include "users.h"
#include "inspircd.h"

void CheckDie();
void LoadAllModules(InspIRCd* ServerInstance);
void CheckRoot();
void OpenLog(char** argv, int argc);
bool DoBackgroundUserStuff(time_t TIME);
void ProcessUser(userrec* cu);

#endif
