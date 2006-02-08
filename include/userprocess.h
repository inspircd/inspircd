#ifndef __USERPROCESS_H__
#define __USERPROCESS_H__

#include "users.h"
#include "inspircd.h"

void CheckDie();
void LoadAllModules(InspIRCd* ServerInstance);
void CheckRoot();
void OpenLog(char** argv, int argc);
void DoBackgroundUserStuff(time_t TIME);
void ProcessUser(userrec* cu);
void DoSocketTimeouts(time_t TIME);

#endif
