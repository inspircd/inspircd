/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Peter Powell <petpow@saberuk.com>
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

bool Thread::Start()
{
	try
	{
		if (thread)
			return false;

		thread = new std::thread(Thread::StartInternal, this);
		return true;
	}
	catch (const std::system_error& err)
	{
		throw CoreException("Unable to start new thread: " + std::string(err.what()));
	}
}

void Thread::StartInternal(Thread* thread)
{
#ifndef _WIN32
	// C++ does not have an API for this so we still need to use pthreads.
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGPIPE);
	pthread_sigmask(SIG_BLOCK, &set, nullptr);
#endif

	thread->OnStart();
}

bool Thread::Stop()
{
	if (!thread)
		return false;

	OnStop();
	stopping = true;
	thread->join();

	delete thread;
	thread = nullptr;
	return true;
}
