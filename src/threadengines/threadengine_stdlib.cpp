/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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


#include "inspircd.h"
#include "threadengines/threadengine_stdlib.h"
#include <stdexcept>

static void entry_point(void* parameter)
{
	Thread* pt = static_cast<Thread*>(parameter);
	pt->Run();
}

void ThreadEngine::Start(Thread* thread)
{
	ThreadData* data = new ThreadData;;
	try
	{
		data->thread = new std::thread(entry_point, thread);
	}
	catch(std::exception e)
	{
		thread->state = NULL;
		delete data;
		throw CoreException("Unable to create new thread: " + std::string(e.what()));
	}
	thread->state = data;
}

void ThreadData::FreeThread(Thread* toFree)
{
	toFree->SetExitFlag();
	thread->join();
}