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

#include <condition_variable>
#include <mutex>

class ThreadSignalSocket;

class CoreExport SocketThread
	: public Thread
{
private:
	std::mutex mutex;
	std::condition_variable_any condvar;
	ThreadSignalSocket* socket;

protected:
	/** Waits for an enqueue operation to complete
	 * You MUST hold the queue lock when you call this.
	 * It will be unlocked while you wait, and will be relocked
	 * before the function returns
	 */
	void WaitForQueue()
	{
		condvar.wait(mutex);
	}
public:
	/** Notifies parent by making the SignalFD ready to read
	 * No requirements on locking
	 */
	void NotifyParent();
	SocketThread();
	~SocketThread() override;
	/** Lock queue.
	 */
	void LockQueue()
	{
		mutex.lock();
	}
	/** Unlock queue.
	 */
	void UnlockQueue()
	{
		mutex.unlock();
	}
	/** Unlock queue and send wakeup to worker
	 */
	void UnlockQueueWakeup()
	{
		condvar.notify_all();
		mutex.unlock();
	}
	void OnStop() override
	{
		mutex.lock();
		condvar.notify_all();
		mutex.unlock();
	}

	/**
	 * Called in the context of the parent thread after a notification
	 * has passed through the socket
	 */
	virtual void OnNotify() = 0;
};
