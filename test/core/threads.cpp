/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 Dennis Friis <peavey@inspircd.org>
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

#include "inspircd_test.h"

class TestSuiteThread : public Thread
{
 public:
	TestSuiteThread() : Thread()
	{
	}

	virtual ~TestSuiteThread()
	{
	}

	virtual void Run()
	{
		while (GetExitFlag() == false)
		{
			//std::cout << "Test suite thread run...\n";
			usleep(50);
		}
	}
};

TEST(threads, test1)
{
	std::string anything;
	ThreadEngine* te = NULL;

	//std::cout << "Creating new ThreadEngine class...\n";
	try
	{
		te = new ThreadEngine;
	}
	catch (...)
	{
		//std::cout << "Creation failed, test failure.\n";
		FAIL();
	}
	//std::cout << "Creation success, type " << te->GetName() << "\n";

	//std::cout << "Allocate: new TestSuiteThread...\n";
	TestSuiteThread* tst = new TestSuiteThread();

	//std::cout << "ThreadEngine::Create on TestSuiteThread...\n";
	try
	{
		try
		{
			te->Start(tst);
		}
		catch (CoreException &ce)
		{
			std::cout << "Failure: " << ce.GetReason() << std::endl;
			FAIL();
		}
	}
	catch (...)
	{
		std::cout << "Failure, unhandled exception\n";
		FAIL();
	}

	usleep(25);

	/* Thread engine auto frees thread on delete */
	//std::cout << "Waiting for thread to exit... " << std::flush;
	delete tst;
	//std::cout << "Done!\n";

	//std::cout << "Delete ThreadEngine... ";
	delete te;
	//std::cout << "Done!\n";
}

