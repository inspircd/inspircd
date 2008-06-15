#include "inspircd.h"
#include "threadengine.h"
#include "inspircd_namedpipe.h"
#include <windows.h>

void IPCThread::Run()
{
	LPTSTR Pipename = "\\\\.\\pipe\\InspIRCdStatus";

	Pipe = CreateNamedPipe ( Pipename,
                                          PIPE_ACCESS_DUPLEX, // read/write access
                                          PIPE_TYPE_MESSAGE | // message type pipe
                                          PIPE_READMODE_MESSAGE | // message-read mode
                                          PIPE_WAIT, // blocking mode
                                          PIPE_UNLIMITED_INSTANCES, // max. instances
                                          MAXBUF, // output buffer size
                                          MAXBUF, // input buffer size
                                          1000, // client time-out
                                          NULL); // no security attribute

	if (Pipe == INVALID_HANDLE_VALUE)
		return;

	while (GetExitFlag() == false)
	{
		// Trying connectnamedpipe in sample for CreateNamedPipe
		// Wait for the client to connect; if it succeeds,
		// the function returns a nonzero value. If the function returns
		// zero, GetLastError returns ERROR_PIPE_CONNECTED.

		Connected = ConnectNamedPipe(Pipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

		if (Connected)
		{
			Success = ReadFile (Pipe, // handle to pipe
				Request, // buffer to receive data
				MAXBUF, // size of buffer
				&BytesRead, // number of bytes read
				NULL); // not overlapped I/O

			Request[BytesRead] = '\0';
			//printf("Data Received: %s\n",chRequest);

			if (!Success || !BytesRead)
				break;

			FlushFileBuffers(Pipe);
			DisconnectNamedPipe(Pipe);
		}
		else
		{
			// The client could not connect.
			CloseHandle(Pipe);
		}

		SleepEx(100, FALSE);
	}
	CloseHandle(Pipe);
}

IPC::IPC(InspIRCd* Srv) : ServerInstance(Srv)
{
	/* The IPC pipe is threaded */
	thread = new IPCThread(Srv);
	Srv->Threads->Create(thread);
}

void IPC::Check()
{
}

IPC::~IPC()
{
	thread->SetExitFlag();
	delete thread;
}