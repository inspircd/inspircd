/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022-2024 Sadie Powell <sadie@sadiepowell.dev>
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

static std::string GetWin32Error()
{
	const auto dwErrorCode = GetLastError();

	auto szErrorString = GetErrorMessage(dwErrorCode);
	for (size_t pos = 0; ((pos = szErrorString.find_first_of("\r\n", pos)) != std::string::npos); )
		szErrorString[pos] = ' ';

	return szErrorString;
}

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
		throw CoreException(GetWin32Error());
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
	g_ServiceStatusHandle = RegisterServiceCtrlHandler(TEXT(INSPIRCD_BRANCH), (LPHANDLER_FUNCTION)ServiceCtrlHandler);
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

/* In windows, our main() flows through here, before calling the 'real' main, smain() in inspircd.cpp */
int main(int argc, char* argv[])
{
	SERVICE_TABLE_ENTRY serviceTable[] =
	{
		{ (LPSTR)INSPIRCD_BRANCH, (LPSERVICE_MAIN_FUNCTION)ServiceMain },
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
