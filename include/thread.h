/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019, 2021 Sadie Powell <sadie@witchery.services>
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


#pragma once

#include <thread>

class CoreExport Thread
{
private:
	/** Whether this thread is in the process of stopping. */
	std::atomic_bool stopping = { false };

	/** The underlying C++ thread. */
	std::thread* thread = nullptr;

	/** Internal callback which sets up the thread and calls OnStart.
	 * @param thread A thread which is starting up.
	 */
	static void StartInternal(Thread* thread);

protected:
	/** Callback which is executed on this thread after it has started. */
	virtual void OnStart() = 0;

	/** Callback which is executed on the calling thread before this thread is stopped. */
	virtual void OnStop() { }

	/** Initialises an instance of the Thread class. */
	Thread() = default;

public:
	/** Destroys an instance of the Thread class. */
	virtual ~Thread() = default;

	/** Determines whether this thread is currently running. */
	bool IsRunning() const { return thread; }

	/** Determines whether this thread is currently stopping. */
	bool IsStopping() const { return stopping.load(); }

	/** Starts the execution of this thread.
	 * @return True if the thread was started, false if it was already running.
	 */
	bool Start();

	/** Stops the execution of this thread.
	 * @return True if the thread was stopped, false if it was already stopped.
	 */
	bool Stop();
};
