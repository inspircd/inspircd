/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *          the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core: libIRCDtestsuite */

#include "inspircd.h"
#include "testsuite.h"
#include "threadengine.h"
#include <iostream>

using namespace std;

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
			cout << "Test suite thread run...\n";
			sleep(5);
		}
	}
};

TestSuite::TestSuite(InspIRCd* Instance) : ServerInstance(Instance)
{
	cout << "\n\n*** STARTING TESTSUITE ***\n";

	std::string modname;
	std::string choice;

	ServerInstance->SE->Blocking(fileno(stdin));

	while (1)
	{
		cout << "(1) Call all module OnRunTestSuite() methods\n";
		cout << "(2) Load a module\n";
		cout << "(3) Unload a module\n";
		cout << "(4) Threading tests\n";

		cout << endl << "(X) Exit test suite\n";

		cout << "\nChoice: ";
		cin >> choice;

		switch (*choice.begin())
		{
			case '1':
				FOREACH_MOD(I_OnRunTestSuite, OnRunTestSuite());
			break;
			case '2':
				cout << "Enter module filename to load: ";
				cin >> modname;
				cout << (Instance->Modules->Load(modname.c_str()) ? "\nSUCCESS!\n" : "\nFAILURE\n");
			break;
			case '3':
				cout << "Enter module filename to unload: ";
				cin >> modname;
				cout << (Instance->Modules->Unload(modname.c_str()) ? "\nSUCCESS!\n" : "\nFAILURE\n");
			break;
			case '4':
				cout << (DoThreadTests() ? "\nSUCCESS!\n" : "\nFAILURE\n");
			break;
			case 'X':
				return;
			break;
			default:
				cout << "Invalid option\n";
			break;
		}
		cout << endl;
	}
}

bool TestSuite::DoThreadTests()
{
	std::string anything;
	ThreadEngine* te = NULL;

	cout << "Creating new ThreadEngine class...\n";
	try
	{
		ThreadEngineFactory* tef = new ThreadEngineFactory();
		te = tef->Create(ServerInstance);
		delete tef;
	}
	catch (...)
	{
		cout << "Creation failed, test failure.\n";
		return false;
	}
	cout << "Creation success, type " << te->GetName() << "\n";

	cout << "Allocate: new TestSuiteThread...\n";
	TestSuiteThread* tst = new TestSuiteThread();

	cout << "ThreadEngine::Create on TestSuiteThread...\n";
	te->Create(tst);

	cout << "Type any line and press enter to end test.\n";
	cin >> anything;

	/* Thread engine auto frees thread on delete */
	cout << "Waiting for thread to exit... " << flush;
	delete tst;
	cout << "Done!\n";

	cout << "Delete ThreadEngine... ";
	delete te;
	cout << "Done!\n";

	return true;
}

TestSuite::~TestSuite()
{
	cout << "\n\n*** END OF TEST SUITE ***\n";
}

