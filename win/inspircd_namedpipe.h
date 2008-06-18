#ifndef INSPIRCD_NAMEDPIPE
#define INSPIRCD_NAMEDPIPE

#include "threadengine.h"
#include <windows.h>

class InspIRCd;

class IPCThread : public Thread
{
	BOOL Connected;
	DWORD BytesRead;
	BOOL Success;
	HANDLE Pipe;
	InspIRCd* ServerInstance;
	char status[MAXBUF];
	int result;
 public:
	IPCThread(InspIRCd* Instance);
	virtual ~IPCThread();
	virtual void Run();
	const char GetStatus();
	int GetResult();
	void ClearStatus();
	void SetResult(int newresult);
};

class IPC
{
 private:
	InspIRCd* ServerInstance;
	IPCThread* thread;
 public:
	IPC(InspIRCd* Srv);
	void Check();
	~IPC();
};

#endif