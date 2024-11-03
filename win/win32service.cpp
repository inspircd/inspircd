/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2021 Herman <GermanAizek@yandex.ru>
 *   Copyright (C) 2013 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
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


#include <windows.h>

#include <fmt/format.h>

#include "inspircd.h"

static SERVICE_STATUS_HANDLE g_ServiceStatusHandle;
static SERVICE_STATUS g_ServiceStatus;
static bool g_bRunningAsService;

struct Service_Data {
	DWORD argc;
	LPSTR* argv;
};

static Service_Data g_ServiceData;

/** The main part of inspircd runs within this thread function. This allows the service part to run
 * separately on its own and to be able to kill the worker thread when its time to quit.
 */
DWORD WINAPI WorkerThread(LPVOID param)
{
	smain(g_ServiceData.argc, g_ServiceData.argv);
	return 0;
}

/* This is called when all startup is done */
void SetServiceRunning()
{
	if (!g_bRunningAsService)
		return;

	g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

	if( !SetServiceStatus( g_ServiceStatusHandle, &g_ServiceStatus ) )
		throw CWin32Exception();
}

/* In windows we hook this to InspIRCd::Exit(EXIT_FAILURE) */
void SetServiceStopped(DWORD dwStatus)
{
	if (!g_bRunningAsService)
		return;

	g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
	if(dwStatus != EXIT_SUCCESS)
	{
		g_ServiceStatus.dwServiceSpecificExitCode = dwStatus;
		g_ServiceStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
	}
	else
	{
		g_ServiceStatus.dwWin32ExitCode = ERROR_SUCCESS;
	}
	SetServiceStatus( g_ServiceStatusHandle, &g_ServiceStatus );
}

/** This callback is called by windows when the state of the service has been changed */
VOID ServiceCtrlHandler(DWORD controlCode)
{
	switch(controlCode)
	{
		case SERVICE_CONTROL_SHUTDOWN:
		case SERVICE_CONTROL_STOP:
			g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
			SetServiceStatus( g_ServiceStatusHandle, &g_ServiceStatus );
			break;
	}
}

/** This callback is called by windows when the service is started */
VOID ServiceMain(DWORD argc, LPCSTR* argv)
{
	g_ServiceStatusHandle = RegisterServiceCtrlHandler(TEXT("InspIRCd"), (LPHANDLER_FUNCTION)ServiceCtrlHandler);
	if( !g_ServiceStatusHandle )
		return;

	g_ServiceStatus.dwCheckPoint = 1;
	g_ServiceStatus.dwControlsAccepted = 0;
	g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_ServiceStatus.dwWaitHint = 5000;
	g_ServiceStatus.dwWin32ExitCode = NO_ERROR;
	g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;

	if( !SetServiceStatus( g_ServiceStatusHandle, &g_ServiceStatus ) )
		return;

	char szModuleName[MAX_PATH];
	if(GetModuleFileNameA(nullptr, szModuleName, MAX_PATH))
	{
		if(!argc)
			argc = 1;

		g_ServiceData.argc = argc;

		// Note: since this memory is going to stay allocated for the rest of the execution,
		// it doesn't make sense to free it, as it's going to be "freed" on process termination
		try {
			g_ServiceData.argv = new char*[argc];

			uint32_t allocsize = strnlen_s(szModuleName, MAX_PATH) + 1;
			g_ServiceData.argv[0] = new char[allocsize];
			strcpy_s(g_ServiceData.argv[0], allocsize, szModuleName);

			for(uint32_t i = 1; i < argc; i++)
			{
				allocsize = strnlen_s(argv[i], MAX_PATH) + 1;
				g_ServiceData.argv[i] = new char[allocsize];
				strcpy_s(g_ServiceData.argv[i], allocsize, argv[i]);
			}

			*(strrchr(szModuleName, '\\') + 1) = '\0';
			SetCurrentDirectoryA(szModuleName);

			HANDLE hThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)WorkerThread, nullptr, 0, nullptr);
			if (hThread != nullptr)
			{
				WaitForSingleObject(hThread, INFINITE);
				CloseHandle(hThread);
			}
		}
		catch(...)
		{
			g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
			g_ServiceStatus.dwWin32ExitCode = ERROR_OUTOFMEMORY;
			SetServiceStatus( g_ServiceStatusHandle, &g_ServiceStatus );
		}
	}
	if(g_ServiceStatus.dwCurrentState == SERVICE_STOPPED)
		return;

	g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
	g_ServiceStatus.dwWin32ExitCode = GetLastError();
	SetServiceStatus( g_ServiceStatusHandle, &g_ServiceStatus );
}

/** Install the windows service. This requires administrator privileges. */
void InstallService()
{
	SC_HANDLE InspServiceHandle = 0, SCMHandle = 0;

	try {
		TCHAR tszBinaryPath[MAX_PATH];
		if(!GetModuleFileName(nullptr, tszBinaryPath, _countof(tszBinaryPath)))
		{
			throw CWin32Exception();
		}

		SCMHandle = OpenSCManager(0, 0, SC_MANAGER_ALL_ACCESS);
		if (!SCMHandle)
		{
			throw CWin32Exception();
		}

		InspServiceHandle = CreateService(SCMHandle, TEXT("InspIRCd"), TEXT("InspIRCd Daemon"), SERVICE_CHANGE_CONFIG, SERVICE_WIN32_OWN_PROCESS,
			SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, tszBinaryPath, 0, 0, 0, TEXT("NT AUTHORITY\\NetworkService"), nullptr);

		if (!InspServiceHandle)
		{
			throw CWin32Exception();
		}

		TCHAR tszDescription[] = TEXT("The InspIRCd service hosts IRC channels and conversations. If this service is stopped, the IRC server will be unavailable.");
		SERVICE_DESCRIPTION svDescription = { tszDescription };
		if(!ChangeServiceConfig2(InspServiceHandle, SERVICE_CONFIG_DESCRIPTION, &svDescription))
		{
			throw CWin32Exception();
		}

		CloseServiceHandle(InspServiceHandle);
		CloseServiceHandle(SCMHandle);
		fmt::println("Service installed.");
	}
	catch(const CWin32Exception& e)
	{
		if(InspServiceHandle)
			CloseServiceHandle(InspServiceHandle);

		if(SCMHandle)
			CloseServiceHandle(SCMHandle);

		fmt::println("Service installation failed: {}", e.what());
	}
}

/** Remove the windows service. This requires administrator privileges. */
void UninstallService()
{
	SC_HANDLE InspServiceHandle = 0, SCMHandle = 0;

	try
	{
		SCMHandle = OpenSCManager(nullptr, SERVICES_ACTIVE_DATABASE, DELETE);
		if (!SCMHandle)
			throw CWin32Exception();

		InspServiceHandle = OpenService(SCMHandle, TEXT("InspIRCd"), DELETE);
		if (!InspServiceHandle)
			throw CWin32Exception();

		if (!DeleteService(InspServiceHandle) && GetLastError() != ERROR_SERVICE_MARKED_FOR_DELETE)
		{
			throw CWin32Exception();
		}

		CloseServiceHandle(InspServiceHandle);
		CloseServiceHandle(SCMHandle);
		fmt::println("Service removed.");
	}
	catch(const CWin32Exception& e)
	{
		if(InspServiceHandle)
			CloseServiceHandle(InspServiceHandle);

		if(SCMHandle)
			CloseServiceHandle(SCMHandle);

		fmt::println("Service deletion failed: {}", e.what());
	}
}

/* In windows, our main() flows through here, before calling the 'real' main, smain() in inspircd.cpp */
int main(int argc, char* argv[])
{
	/* Check for parameters */
	if (argc > 1)
	{
		for (int i = 1; i < argc; i++)
		{
			if(!_stricmp(argv[i], "--installservice"))
			{
				InstallService();
				return EXIT_SUCCESS;
			}
			if(!_stricmp(argv[i], "--uninstallservice") || !_stricmp(argv[i], "--removeservice"))
			{
				UninstallService();
				return EXIT_SUCCESS;
			}
		}
	}

	SERVICE_TABLE_ENTRY serviceTable[] =
	{
		{ (LPSTR)"InspIRCd", (LPSERVICE_MAIN_FUNCTION)ServiceMain },
		{ nullptr, nullptr }
	};

	g_bRunningAsService = true;
	if( !StartServiceCtrlDispatcher(serviceTable) )
	{
		// This error means that the program was not started as service.
		if( GetLastError() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT )
		{
			g_bRunningAsService = false;
			return smain(argc, argv);
		}
		else
		{
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
