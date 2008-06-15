#include "inspircd.h"
#include "threadengine.h"
#include "inspircd_namedpipe.h"
#include <windows.h>

void IPCThread::Run()
{

	printf("*** IPCThread::Run() *** \n");
	LPTSTR Pipename = "\\\\.\\pipe\\InspIRCdStatus";

	while (GetExitFlag() == false)
	{
		Pipe = CreateNamedPipe (Pipename,
                                          PIPE_ACCESS_DUPLEX, // read/write access
                                          PIPE_TYPE_MESSAGE | // message type pipe
                                          PIPE_READMODE_MESSAGE | // message-read mode
                                          PIPE_WAIT, // blocking mode
                                          PIPE_UNLIMITED_INSTANCES, // max. instances
                                          MAXBUF, // output buffer size
                                          MAXBUF, // input buffer size
                                          1000, // client time-out
                                          NULL); // no security attribute

		printf("*** After CreateNamedPipe *** \n");

		if (Pipe == INVALID_HANDLE_VALUE)
		{
			printf("*** IPC failure creating named pipe: %s\n", dlerror());
			return;
		}

		printf("*** After check, exit flag=%d *** \n", GetExitFlag());

		printf("*** Loop *** \n");
		Connected = ConnectNamedPipe(Pipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

		printf("*** After ConnectNamedPipe *** \n");

		if (Connected)
		{
			ServerInstance->Logs->Log("IPC", DEBUG, "About to ReadFile from pipe");

			Success = ReadFile (Pipe, // handle to pipe
				Request, // buffer to receive data
				MAXBUF, // size of buffer
				&BytesRead, // number of bytes read
				NULL); // not overlapped I/O

			Request[BytesRead] = '\0';
			ServerInstance->Logs->Log("IPC", DEBUG, "Received from IPC: %s", Request);
			//printf("Data Received: %s\n",chRequest);

			if (!Success || !BytesRead)
			{
				printf("*** IPC failure reading client named pipe: %s\n", dlerror());
				continue;
			}

			std::stringstream status;
			DWORD Written = 0;
			ServerInstance->Threads->Mutex(true);

			status << "name " << ServerInstance->Config->ServerName << std::endl;
			status << "END" << std::endl;

			ServerInstance->Threads->Mutex(false);

			/* This is a blocking call and will succeed, so long as the client doesnt disconnect */
			Success = WriteFile(Pipe, status.str().data(), status.str().length(), &Written, NULL);

			FlushFileBuffers(Pipe);
			DisconnectNamedPipe(Pipe);
		}
		else
		{
			// The client could not connect.
			printf("*** IPC failure connecting named pipe: %s\n", dlerror());
		}

		printf("*** sleep for next client ***\n");
		printf("*** Closing pipe handle\n");
		CloseHandle(Pipe);
	}
}

IPC::IPC(InspIRCd* Srv) : ServerInstance(Srv)
{
	/* The IPC pipe is threaded */
	thread = new IPCThread(Srv);
	Srv->Threads->Create(thread);
	printf("*** CREATE IPC THREAD ***\n");
}

void IPC::Check()
{
	ServerInstance->Threads->Mutex(true);

	/* Check the state of the thread, safe in here */



	ServerInstance->Threads->Mutex(false);
}

IPC::~IPC()
{
	thread->SetExitFlag();
	delete thread;
}