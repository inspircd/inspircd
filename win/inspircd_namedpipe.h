/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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