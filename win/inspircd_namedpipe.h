#ifndef INSPIRCD_NAMEDPIPE
#define INSPIRCD_NAMEDPIPE

#include "threadengine.h"
#include <windows.h>

class IPCThread : public Thread
{
	BOOL Connected;
	DWORD BytesRead;
	BOOL Success;
	HANDLE Pipe;
	char status[MAXBUF];
	int result;
 public:
	IPCThread();
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
	IPCThread* thread;
 public:
	IPC();
	void Check();
	~IPC();
};

#endif
