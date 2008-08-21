/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern int smain(int argc, char** argv);
extern const char* dlerror();

static SERVICE_STATUS_HANDLE serviceStatusHandle;
static HANDLE hThreadEvent;
static HANDLE killServiceEvent;
static int serviceCurrentStatus;

// This is used to define ChangeServiceConf2() as we can't link
// directly against this symbol (see below where it is used)
typedef BOOL (CALLBACK* SETSERVDESC)(SC_HANDLE,DWORD,LPVOID);
typedef void (*CommandlineParameterHandler)(void);

SETSERVDESC ChangeServiceConf;		// A function pointer for dynamic linking tricks


void KillService()
{
	/* FIXME: This should set a flag in the mainloop for shutting down */
	SetEvent(hThreadEvent);
	Sleep(2000);
	SetEvent(killServiceEvent);
}

DWORD WINAPI WorkerThread(LPDWORD param)
{
	// *** REAL MAIN HERE ***
	char modname[MAX_PATH];
	GetModuleFileName(NULL, modname, sizeof(modname));
	char* argv[] = { modname, "--nofork", "--debug" };
	smain(3, argv);
	KillService();
	return 0;
}

void StartServiceThread()
{
	DWORD dwd;
	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)WorkerThread,NULL,0,&dwd);
}


BOOL UpdateSCMStatus (DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwServiceSpecificExitCode, DWORD dwCheckPoint, DWORD dwWaitHint)
{
	BOOL success;
	SERVICE_STATUS serviceStatus;
	serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	serviceStatus.dwCurrentState = dwCurrentState;

	if (dwCurrentState == SERVICE_START_PENDING)
	{
		serviceStatus.dwControlsAccepted = 0;
	}
	else
	{
		serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	}

	if (dwServiceSpecificExitCode == 0)
	{
		serviceStatus.dwWin32ExitCode = dwWin32ExitCode;
	}
	else
	{
		serviceStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
	}
	serviceStatus.dwServiceSpecificExitCode =   dwServiceSpecificExitCode;
	serviceStatus.dwCheckPoint = dwCheckPoint;
	serviceStatus.dwWaitHint = dwWaitHint;

	success = SetServiceStatus (serviceStatusHandle, &serviceStatus);
	if (!success)
	{
		KillService();
	}
	return success;
}


void terminateService (int code, int wincode)
{
	UpdateSCMStatus(SERVICE_STOPPED,wincode?wincode:ERROR_SERVICE_SPECIFIC_ERROR,(wincode)?0:code,0,0);
	return;
}


VOID ServiceCtrlHandler (DWORD controlCode)
{
	switch(controlCode)
	{
		case SERVICE_CONTROL_INTERROGATE:
		break;
		case SERVICE_CONTROL_SHUTDOWN:
		case SERVICE_CONTROL_STOP:
			serviceCurrentStatus = SERVICE_STOP_PENDING;
			UpdateSCMStatus(SERVICE_STOP_PENDING, NO_ERROR, 0, 1, 5000);
			KillService();
			UpdateSCMStatus(SERVICE_STOPPED, NO_ERROR, 0, 0, 0);
			return;
		default:
		break;
	}
	UpdateSCMStatus(serviceCurrentStatus, NO_ERROR, 0, 0, 0);
}


VOID ServiceMain(DWORD argc, LPTSTR *argv)
{
	BOOL success;
	DWORD type=0, size=0;

	serviceStatusHandle = RegisterServiceCtrlHandler("InspIRCd", (LPHANDLER_FUNCTION)ServiceCtrlHandler);
	if (!serviceStatusHandle)
	{
		terminateService(1, GetLastError());
		return;
	}

	success = UpdateSCMStatus(SERVICE_START_PENDING, NO_ERROR, 0, 1, 1000);
	if (!success)
	{
		terminateService(2, GetLastError());
		return;
	}

	killServiceEvent = CreateEvent(NULL,true,false,NULL);
	hThreadEvent = CreateEvent(NULL,true,false,NULL);

	if (!killServiceEvent || !hThreadEvent)
	{
		terminateService(99, GetLastError());
		return;
	}

	success = UpdateSCMStatus(SERVICE_START_PENDING, NO_ERROR, 0, 2, 1000);
	if (!success)
	{
		terminateService(2, GetLastError());
		return;
	}

	StartServiceThread();
	serviceCurrentStatus = SERVICE_RUNNING;
	success = UpdateSCMStatus(SERVICE_RUNNING, NO_ERROR, 0, 0, 0);
	if (!success)
	{
		terminateService(6, GetLastError());
		return;
	}
	WaitForSingleObject (killServiceEvent, INFINITE);
}

void InstallService()
{
	SC_HANDLE myService, scm;
	SERVICE_DESCRIPTION svDesc;
	HINSTANCE advapi32;

	char modname[MAX_PATH];
	GetModuleFileName(NULL, modname, sizeof(modname));

	scm = OpenSCManager(0,0,SC_MANAGER_CREATE_SERVICE);
	if (!scm)
	{
		printf("Unable to open service control manager: %s\n", dlerror());
		return;
	}

	myService = CreateService(scm,"InspIRCd","Inspire IRC Daemon", SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
		SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, modname, 0, 0, 0, NULL, NULL);

	if (!myService)
	{
		printf("Unable to create service: %s\n", dlerror());
		CloseServiceHandle(scm);
		return;
	}

	// *** Set service description ***
	// this is supported from 5.0 (win2k) onwards only, so we can't link to the definition of
	// this function in advapi32.lib, otherwise the program will not run on windows NT 4. We
	// must use LoadLibrary and GetProcAddress to export the function name from advapi32.dll
	advapi32 = LoadLibrary("advapi32.dll");
	if (advapi32)
	{
		ChangeServiceConf = (SETSERVDESC)GetProcAddress(advapi32,"ChangeServiceConfig2A");
		if (ChangeServiceConf)
		{
			char desc[] = "The Inspire Internet Relay Chat Daemon hosts IRC channels and conversations.\
 If this service is stopped, the IRC server will not run.";
			svDesc.lpDescription = desc;
			BOOL success = ChangeServiceConf(myService,SERVICE_CONFIG_DESCRIPTION, &svDesc);
			if (!success)
			{
				printf("Unable to set service description: %s\n", dlerror());
				CloseServiceHandle(myService);
				CloseServiceHandle(scm);
				return;
			}
		}
		FreeLibrary(advapi32);
	}

	printf("Service installed.\n");
	CloseServiceHandle(myService);
	CloseServiceHandle(scm);
}

void RemoveService()
{
	SC_HANDLE myService, scm;

	scm = OpenSCManager(0,0,SC_MANAGER_CREATE_SERVICE);
	if (!scm)
	{
		printf("Unable to open service control manager: %s\n", dlerror());
		return;
	}

	myService = OpenService(scm,"InspIRCd",SERVICE_ALL_ACCESS);
	if (!myService)
	{
		printf("Unable to open service: %s\n", dlerror());
		CloseServiceHandle(scm);
		return;
	}

	if (!DeleteService(myService))
	{
		printf("Unable to delete service: %s\n", dlerror());
		CloseServiceHandle(myService);
		CloseServiceHandle(scm);
		return;
	}

	printf("Service removed.\n");
	CloseServiceHandle(myService);
	CloseServiceHandle(scm);
}

struct Commandline
{
	const char* Switch;
	CommandlineParameterHandler Handler;
};

/* In windows, our main() flows through here, before calling the 'real' main, smain() in inspircd.cpp */
int main(int argc, char** argv)
{

	Commandline params[] = {
		{ "--installservice", InstallService },
		{ "--removeservice", RemoveService },
		{ NULL }
	};

	/* Check for parameters */
	if (argc > 1)
	{
		for (int z = 0; params[z].Switch; ++z)
		{
			if (!_stricmp(argv[1], params[z].Switch))
			{
				params[z].Handler();
				return 0;
			}
		}
	}

	/* First, check if the service is installed.
	 * if it is not, or we're starting as non-administrator,
	 * just call smain() and start as normal non-service
	 * process.
	 */
	SC_HANDLE myService, scm;
	scm = OpenSCManager(0,0,SC_MANAGER_CREATE_SERVICE);
	if (scm)
	{
		myService = OpenService(scm,"InspIRCd",SERVICE_ALL_ACCESS);
		if (!myService)
		{
			/* Service not installed or no permission to modify it */
			CloseServiceHandle(scm);
			smain(argc, argv);
		}
	}
	else
	{
		/* Not enough privileges to open the SCM */
		smain(argc, argv);
	}

	CloseServiceHandle(myService);
	CloseServiceHandle(scm);

	/* If we get here, we know the service is installed so we can start it */

	SERVICE_TABLE_ENTRY serviceTable[] =
	{
		{"InspIRCd", (LPSERVICE_MAIN_FUNCTION) ServiceMain },
		{NULL, NULL}
	};

	StartServiceCtrlDispatcher(serviceTable);
	return 0;
}
