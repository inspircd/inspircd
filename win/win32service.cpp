/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "inspircd_config.h"
#include "inspircd.h"
#include "exitcodes.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static SERVICE_STATUS_HANDLE serviceStatusHandle;
static HANDLE hThreadEvent;
static HANDLE killServiceEvent;
static int serviceCurrentStatus;

/** This is used to define ChangeServiceConf2() as we can't link
 * directly against this symbol (see below where it is used)
 */
typedef BOOL (CALLBACK* SETSERVDESC)(SC_HANDLE,DWORD,LPVOID);

BOOL UpdateSCMStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwServiceSpecificExitCode, DWORD dwCheckPoint, DWORD dwWaitHint);
void terminateService(int code, int wincode);

/* A commandline parameter handler for service specific commandline parameters */
typedef void (*CommandlineParameterHandler)(void);

/* Represents a commandline and its handler */
struct Commandline
{
	const char* Switch;
	CommandlineParameterHandler Handler;
};

/* A function pointer for dynamic linking tricks */
SETSERVDESC ChangeServiceConf;

/* Returns true if this program is running as a service, false if it is running interactive */
bool IsAService()
{
	USEROBJECTFLAGS uoflags;
	HWINSTA winstation = GetProcessWindowStation();
	if (GetUserObjectInformation(winstation, UOI_FLAGS, &uoflags, sizeof(uoflags), NULL))
		return ((uoflags.dwFlags & WSF_VISIBLE) == 0);
	else
		return false;
}

/* Kills the service by setting an event which the other thread picks up and exits */
void KillService()
{
	SetEvent(hThreadEvent);
	Sleep(2000);
	SetEvent(killServiceEvent);
}

/** The main part of inspircd runs within this thread function. This allows the service part to run
 * seperately on its own and to be able to kill the worker thread when its time to quit.
 */
DWORD WINAPI WorkerThread(LPDWORD param)
{
	char modname[MAX_PATH];
	GetModuleFileName(NULL, modname, sizeof(modname));
	char* argv[] = { modname, "--nofork" };
	smain(2, argv);
	KillService();
	return 0;
}

/* This is called when all startup is done */
void SetServiceRunning()
{
	if (!IsAService())
		return;

	serviceCurrentStatus = SERVICE_RUNNING;
	BOOL success = UpdateSCMStatus(SERVICE_RUNNING, NO_ERROR, 0, 0, 0);
	if (!success)
	{
		terminateService(EXIT_STATUS_UPDATESCM_FAILED, GetLastError());
		return;
	}
}


/** Starts the worker thread above */
void StartServiceThread()
{
	DWORD dwd;
	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)WorkerThread,NULL,0,&dwd);
}

/** This function updates the status of the service in the SCM
 * (service control manager, the services.msc applet)
 */
BOOL UpdateSCMStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwServiceSpecificExitCode, DWORD dwCheckPoint, DWORD dwWaitHint)
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

/** This function is called by us when the service is being shut down or when it can't be started */
void terminateService(int code, int wincode)
{
	UpdateSCMStatus(SERVICE_STOPPED, wincode ? wincode : ERROR_SERVICE_SPECIFIC_ERROR, wincode ? 0 : code, 0, 0);
	return;
}

/* In windows we hook this to InspIRCd::Exit() */
void SetServiceStopped(int status)
{
	if (!IsAService())
		exit(status);

	/* Are we running as a service? If so, trigger the service specific exit code */
	terminateService(status, 0);
	KillService();
	exit(status);
}

/** This callback is called by windows when the state of the service has been changed */
VOID ServiceCtrlHandler(DWORD controlCode)
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

/** This callback is called by windows when the service is started */
VOID ServiceMain(DWORD argc, LPTSTR *argv)
{
	BOOL success;

	serviceStatusHandle = RegisterServiceCtrlHandler("InspIRCd", (LPHANDLER_FUNCTION)ServiceCtrlHandler);
	if (!serviceStatusHandle)
	{
		terminateService(EXIT_STATUS_RSCH_FAILED, GetLastError());
		return;
	}

	success = UpdateSCMStatus(SERVICE_START_PENDING, NO_ERROR, 0, 1, 1000);
	if (!success)
	{
		terminateService(EXIT_STATUS_UPDATESCM_FAILED, GetLastError());
		return;
	}

	killServiceEvent = CreateEvent(NULL, true, false, NULL);
	hThreadEvent = CreateEvent(NULL, true, false, NULL);

	if (!killServiceEvent || !hThreadEvent)
	{
		terminateService(EXIT_STATUS_CREATE_EVENT_FAILED, GetLastError());
		return;
	}

	success = UpdateSCMStatus(SERVICE_START_PENDING, NO_ERROR, 0, 2, 1000);
	if (!success)
	{
		terminateService(EXIT_STATUS_UPDATESCM_FAILED, GetLastError());
		return;
	}

	StartServiceThread();
	WaitForSingleObject (killServiceEvent, INFINITE);
}

/** Install the windows service. This requires administrator privileges. */
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

/** Remove the windows service. This requires administrator privileges. */
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

/* In windows, our main() flows through here, before calling the 'real' main, smain() in inspircd.cpp */
int main(int argc, char** argv)
{
	/* List of parameters and handlers */
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
	scm = OpenSCManager(0,0,GENERIC_READ);
	if (scm)
	{
		myService = OpenService(scm,"InspIRCd",GENERIC_READ);
		if (!myService)
		{
			/* Service not installed or no permission to modify it */
			CloseServiceHandle(scm);
			return smain(argc, argv);
		}
	}
	else
	{
		/* Not enough privileges to open the SCM */
		return smain(argc, argv);
	}

	CloseServiceHandle(myService);
	CloseServiceHandle(scm);

	/* Check if the process is running interactively. InspIRCd does not run interactively
	 * as a service so if this is true, we just run the non-service inspircd.
	 */
	if (!IsAService())
		return smain(argc, argv);

	/* If we get here, we know the service is installed so we can start it */

	SERVICE_TABLE_ENTRY serviceTable[] =
	{
		{"InspIRCd", (LPSERVICE_MAIN_FUNCTION) ServiceMain },
		{NULL, NULL}
	};

	StartServiceCtrlDispatcher(serviceTable);
	return 0;
}
