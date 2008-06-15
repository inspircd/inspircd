#ifndef INSPIRCD_NAMEDPIPE
#define INSPIRCD_NAMEDPIPE

#include "threadengine.h"
#include <windows.h>

class InspIRCd;

class IPCThread : public Thread
{
	BOOL Connected;
	CHAR Request[MAXBUF];
	DWORD BytesRead;
	BOOL Success;
	HANDLE Pipe;
	InspIRCd* ServerInstance;
 public:
	IPCThread(InspIRCd* Instance) : Thread(), ServerInstance(Instance)
	{
	}

	virtual ~IPCThread()
	{
	}

	virtual void Run();
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