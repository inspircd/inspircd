#include "inspircd.h"
#include "threadengine.h"
#include "inspircd_namedpipe.h"
#include "exitcodes.h"
#include <windows.h>
#include <psapi.h>


IPCThread::IPCThread(InspIRCd* Instance) : Thread(), ServerInstance(Instance)
{
}

IPCThread::~IPCThread()
{

}

void IPCThread::Run()
{
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

		if (Pipe == INVALID_HANDLE_VALUE)
		{
			SleepEx(500, true);
			continue;
		}

		Connected = ConnectNamedPipe(Pipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

		if (Connected)
		{
			Success = ReadFile (Pipe, // handle to pipe
				this->status, // buffer to receive data
				1, // size of buffer
				&BytesRead, // number of bytes read
				NULL); // not overlapped I/O

			if (!Success || !BytesRead)
			{
				CloseHandle(Pipe);
				continue;
			}

			const char oldrequest = this->GetStatus();

			/* Wait for main thread to pick up status change */
			while (this->GetStatus())
				SleepEx(10, true);

			std::stringstream stat;
			DWORD Written = 0;

			PROCESS_MEMORY_COUNTERS MemCounters;

			bool HaveMemoryStats = GetProcessMemoryInfo(GetCurrentProcess(), &MemCounters, sizeof(MemCounters));

			stat << "name " << ServerInstance->Config->ServerName << std::endl;
			stat << "gecos " << ServerInstance->Config->ServerDesc << std::endl;
			stat << "numlocalusers " << ServerInstance->Users->LocalUserCount() << std::endl;
			stat << "numusers " << ServerInstance->Users->clientlist->size() << std::endl;
			stat << "numchannels " << ServerInstance->chanlist->size() << std::endl;
			stat << "numopers " << ServerInstance->Users->OperCount() << std::endl;
			stat << "timestamp " << ServerInstance->Time() << std::endl;
			stat << "pid " << GetProcessId(GetCurrentProcess()) << std::endl;
			stat << "request " << oldrequest << std::endl;
			stat << "result " << this->GetResult() << std::endl;
			if (HaveMemoryStats)
			{
				stat << "workingset " << MemCounters.WorkingSetSize << std::endl;
				stat << "pagefile " << MemCounters.PagefileUsage << std::endl;
				stat << "pagefaults " << MemCounters.PageFaultCount << std::endl;
			}

			stat << "END" << std::endl;

			/* This is a blocking call and will succeed, so long as the client doesnt disconnect */
			Success = WriteFile(Pipe, stat.str().data(), stat.str().length(), &Written, NULL);

			FlushFileBuffers(Pipe);
			DisconnectNamedPipe(Pipe);
		}
		CloseHandle(Pipe);
	}
}

const  char IPCThread::GetStatus()
{
	return *status;
}

void IPCThread::ClearStatus()
{
	*status = '\0';
}

int IPCThread::GetResult()
{
	return result;
}

void IPCThread::SetResult(int newresult)
{
	result = newresult;
}


IPC::IPC(InspIRCd* Srv) : ServerInstance(Srv)
{
	/* The IPC pipe is threaded */
	thread = new IPCThread(Srv);
	Srv->Threads->Create(thread);
}

void IPC::Check()
{
	switch (thread->GetStatus())
	{
		case 'N':
			/* No-Operation */
			thread->SetResult(0);
			thread->ClearStatus();
		break;
		case '1':
			/* Rehash */
			ServerInstance->Rehash("due to rehash command from GUI");
			thread->SetResult(0);
			thread->ClearStatus();
		break;
		case '2':
			/* Shutdown */
			thread->SetResult(0);
			thread->ClearStatus();
			ServerInstance->Exit(EXIT_STATUS_NOERROR);
		break;
		case '3':
			/* Restart */
			thread->SetResult(0);
			thread->ClearStatus();
			ServerInstance->Restart("Restarting due to command from GUI");
		break;
	}
}

IPC::~IPC()
{
	thread->SetExitFlag();
	delete thread;
}